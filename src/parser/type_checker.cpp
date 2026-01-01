#include "type_checker.h"
#include <cassert>

namespace parser {

struct TypeError : public std::runtime_error
{
    explicit TypeError(std::string_view msg)
    : std::runtime_error(std::string(msg))
    {
    }
};

static Type getCommonType(const Type &first, const Type &second)
{
    assert(first.isInitialized() && second.isInitialized());
    if (first.t == second.t)
        return first;
    if (first.isBasic(Double) || second.isBasic(Double))
        return Type{ BasicType::Double };
    if (first.size() == second.size())
        return first.isSigned() ? second : first;
    return first.size() > second.size() ? first : second;
}

static std::unique_ptr<Expression> explicitCast(
    std::unique_ptr<Expression> expr,
    const Type &from_type,
    const Type &to_type)
{
    assert(from_type.isInitialized() && to_type.isInitialized());
    if (from_type == to_type)
        return expr;
    auto ret = std::make_unique<Expression>(CastExpression{
        .target_type = to_type,
        .expr = std::move(expr),
        .inner_type = from_type,
    });
    return ret;
}

// This is a terrible hack. When a double value stands as a conditional
// expression, we double-negate it just to wrap it into an expression, which
// can be processed during the assembly generation. There are logics implemented
// in ASM to handle that case where the double is a NaN value.
// TODO: Maybe wrap it into a "pseudo-expression" to optimize it out in assembly?
static std::unique_ptr<Expression> notNot(std::unique_ptr<Expression> expr)
{
    auto ret = std::make_unique<Expression>(UnaryExpression{
        .op = UnaryOperator::Not,
        .expr = std::make_unique<Expression>(UnaryExpression{
            .op = UnaryOperator::Not,
            .expr = std::move(expr),
            .type = Type{ BasicType::Int }
        }),
        .type = Type{ BasicType::Int }
    });
    return ret;
}

Type TypeChecker::operator()(ConstantExpression &c)
{
    c.type = getType(c.value);
    return c.type;
}

Type TypeChecker::operator()(VariableExpression &v)
{
    if (const SymbolEntry *entry = m_symbolTable->get(v.identifier)) {
        if (entry->type.getAs<FunctionType>())
            Abort(std::format("Function name '{}' is used as variable", v.identifier));
        v.type = entry->type;
    } else
        Abort(std::format("Undeclared variable '{}'", v.identifier));
    return v.type;
}

Type TypeChecker::operator()(CastExpression &c)
{
    c.inner_type = Type{ std::visit(*this, *c.expr) };
    // target_type is already determined in the AST builder
    return c.target_type;
}

Type TypeChecker::operator()(UnaryExpression &u)
{
    Type type = std::visit(*this, *u.expr);

    if (type.isBasic(Double) && u.op == UnaryOperator::BitwiseComplement)
        Abort("The type of a unary bitwise complement operation can't be double.");

    if (u.op == UnaryOperator::Not)
        u.type = Type{ BasicType::Int };
    else
        u.type = type;
    return u.type;
}

Type TypeChecker::operator()(BinaryExpression &b)
{
    Type left_type = std::visit(*this, *b.lhs);
    Type right_type = std::visit(*this, *b.rhs);

    if (b.op == BinaryOperator::And || b.op == BinaryOperator::Or) {
        // Return value of logical operators can be represented as an integer
        b.type = Type{ BasicType::Int };
        return b.type;
    }

    if (left_type.isBasic(Double) || right_type.isBasic(Double)) {
        if (b.op == BinaryOperator::Remainder
            || b.op == BinaryOperator::LeftShift
            || b.op == BinaryOperator::RightShift
            || b.op == BinaryOperator::BitwiseAnd
            || b.op == BinaryOperator::BitwiseXor
            || b.op == BinaryOperator::BitwiseOr)
            Abort("The type of the binary operation can't be double.");
    }

    if (b.op == LeftShift || b.op == RightShift) {
        // The right operand of shift operators need an integer promotion
        b.rhs = explicitCast(std::move(b.rhs), right_type, Type{ BasicType::Int });
        b.type = left_type;
        return b.type;
    }

    Type common_type = getCommonType(left_type, right_type);
    b.lhs = explicitCast(std::move(b.lhs), left_type, common_type);
    b.rhs = explicitCast(std::move(b.rhs), right_type, common_type);
    if (isRelationOperator(b.op)) {
        // Represented as integer, but they needed a common type before
        b.type = Type{ BasicType::Int };
        return b.type;
    } else if (isAssignment(b.op))
        b.type = left_type;
    else
        b.type = common_type;
    return b.type;
}

Type TypeChecker::operator()(AssignmentExpression &a)
{
    Type left_type = std::visit(*this, *a.lhs);
    Type right_type = std::visit(*this, *a.rhs);
    a.rhs = explicitCast(std::move(a.rhs), right_type, left_type);
    a.type = left_type;
    return a.type;
}

Type TypeChecker::operator()(ConditionalExpression &c)
{
    Type condition_type = std::visit(*this, *c.condition);
    if (condition_type.isBasic(Double))
        c.condition = notNot(std::move(c.condition));
    Type true_type = std::visit(*this, *c.trueBranch);
    Type false_type = std::visit(*this, *c.falseBranch);
    Type common_type = getCommonType(true_type, false_type);
    c.trueBranch = explicitCast(std::move(c.trueBranch), true_type, common_type);
    c.falseBranch = explicitCast(std::move(c.falseBranch), false_type, common_type);
    c.type = common_type;
    return c.type;
}

Type TypeChecker::operator()(FunctionCallExpression &f)
{
    if (const FunctionType *type = m_symbolTable->getTypeAs<FunctionType>(f.identifier)) {
        if (type->params.size() != f.args.size())
            Abort(std::format("Function '{}' is called with wrong number of arguments", f.identifier));

        for (size_t i = 0; i < f.args.size(); i++) {
            Type arg_type = std::visit(*this, *f.args[i]);
            f.args[i] = explicitCast(std::move(f.args[i]), arg_type, *type->params[i]);
        }
        f.type = type->ret;
        return *f.type;
    } else {
        // The symbol name exists, we verified it during the semantic analysis.
        Abort(std::format("'{}' is not a function name", f.identifier));
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DereferenceExpression &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(AddressOfExpression &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ReturnStatement &r)
{
    Type ret_type = std::visit(*this, *r.expr);
    r.expr = explicitCast(
        std::move(r.expr),
        ret_type,
        *std::get<FunctionType>(m_functionTypeStack.back().t).ret);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(IfStatement &i)
{
    Type condition_type = std::visit(*this, *i.condition);
    if (condition_type.isBasic(Double))
        i.condition = notNot(std::move(i.condition));
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch)
        std::visit(*this, *i.falseBranch);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(GotoStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(LabeledStatement &l)
{
    std::visit(*this, *l.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(BlockStatement &b)
{
    for (auto &i : b.items)
        std::visit(*this, i);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(NullStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(BreakStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ContinueStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(WhileStatement &w)
{
    Type condition_type = std::visit(*this, *w.condition);
    if (condition_type.isBasic(Double))
        w.condition = notNot(std::move(w.condition));
    std::visit(*this, *w.body);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DoWhileStatement &d)
{
    std::visit(*this, *d.body);
    Type condition_type = std::visit(*this, *d.condition);
    if (condition_type.isBasic(Double))
        d.condition = notNot(std::move(d.condition));
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ForStatement &f)
{
    if (f.init) {
        m_forLoopInitializer = true;
        std::visit(*this, *f.init);
        m_forLoopInitializer = false;
    }
    if (f.condition) {
        Type condition_type = std::visit(*this, *f.condition);
        if (condition_type.isBasic(Double))
            f.condition = notNot(std::move(f.condition));
    }
    if (f.update)
        std::visit(*this, *f.update);
    std::visit(*this, *f.body);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(SwitchStatement &s)
{
    m_switches.push_back(&s);
    s.type = std::visit(*this, *s.condition);

    if (s.type.isBasic(Double))
        Abort("The type of a switch statement can't be double.");

    std::visit(*this, *s.body);
    m_switches.pop_back();
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(CaseStatement &c)
{
    Type type = std::visit(*this, *c.condition);

    if (type.isBasic(Double))
        Abort("The type of a case statement can't be double.");

    if (auto expr = std::get_if<ConstantExpression>(c.condition.get())) {
        SwitchStatement *s = m_switches.back();
        // We use the converted value to create the label
        auto value = ConvertValue(expr->value, s->type);
        c.label = std::format("case_{}_{}", s->label, toLabel(value));
        auto [it, inserted] = s->cases.insert(value);
        if (!inserted)
            Abort("Duplicate case in switch");
    }
    std::visit(*this, *c.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DefaultStatement &d)
{
    std::visit(*this, *d.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(FunctionDeclaration &f)
{
    if (!m_fileScope && f.storage == StorageStatic)
        Abort(std::format("Function '{}' can't be declared as static in block scope", f.name));

    bool already_defined = false;
    bool is_global = f.storage != StorageStatic;

    if (const SymbolEntry *entry = m_symbolTable->get(f.name)) {
        if (entry->type != f.type)
            Abort(std::format("Incompatible function declarations of '{}'", f.name));
        if (entry->attrs.defined && f.body)
            Abort(std::format("Function '{}' is defined more than once", f.name));
        already_defined = entry->attrs.defined;
        if (entry->attrs.global && f.storage == StorageStatic)
            Abort(std::format("Static function declaration '{}' follows non-static", f.name));
        is_global = entry->attrs.global;
    }

    m_symbolTable->insert(f.name, f.type, IdentifierAttributes{
        .type = IdentifierAttributes::Function,
        .defined = already_defined || (bool)f.body,
        .global = is_global
    });

    if (f.body) {
        auto params_types = std::get<FunctionType>(f.type.t).params;
        for (size_t i = 0; i < f.params.size(); i++) {
            m_symbolTable->insert(f.params[i], *params_types[i], IdentifierAttributes{
                .type = IdentifierAttributes::Local,
                .defined = false
            });
        }
        m_fileScope = false;
        m_functionTypeStack.push_back(f.type);
        std::visit(*this, *f.body);
        m_functionTypeStack.pop_back();
    }

    return Type{ std::monostate() };
}

Type TypeChecker::operator()(VariableDeclaration &v)
{
    if (m_forLoopInitializer && v.storage != StorageDefault)
        Abort("Initializer of a for loop can't have storage specifier");

    if (m_fileScope) {
        // File-scope variable
        InitialValue init = NoInitializer{};
        if (!v.init) {
            if (v.storage == StorageExtern)
                init = NoInitializer{};
            else
                init = Tentative{};
        } else if (auto n = std::get_if<ConstantExpression>(v.init.get()))
            init = Initial{ .i = ConvertValue(n->value, v.type) };
        else
            Abort(std::format("Non-constant initializer of '{}'", v.identifier));

        bool is_global = (v.storage != StorageStatic);

        if (const SymbolEntry *entry = m_symbolTable->get(v.identifier)) {
            if (entry->type != v.type)
                Abort(std::format("'{}' redeclared with different type", v.identifier));

            if (v.storage == StorageExtern)
                is_global = entry->attrs.global;
            else if (entry->attrs.global != is_global)
                Abort(std::format("Conflicting variable linkage ('{}')", v.identifier));

            if (std::get_if<Initial>(&entry->attrs.init)) {
                if (std::get_if<Initial>(&init))
                    Abort(std::format("Conflicting file scope variable definition ('{}')", v.identifier));
                else
                    init = entry->attrs.init;
            } else if (!std::holds_alternative<Initial>(init) && std::holds_alternative<Tentative>(entry->attrs.init))
                init = Tentative{};
        }

        m_symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
            .type = IdentifierAttributes::Static,
            .global = is_global,
            .init = init
        });
    } else {
        // Block-level variable
        if (v.storage == StorageExtern) {
            if (v.init)
                Abort(std::format("Initializer on local extern variable '{}'", v.identifier));

            if (const SymbolEntry *entry = m_symbolTable->get(v.identifier)) {
                if (entry->type != v.type)
                    Abort(std::format("'{}' redeclared with different type", v.identifier));
            } else {
                m_symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                    .type = IdentifierAttributes::Static,
                    .global = true,
                    .init = NoInitializer{}
                });
            }
        } else if (v.storage == StorageStatic) {
            InitialValue init = NoInitializer{};
            if (!v.init)
                init = Initial{ .i = 0 };
            else if (auto n = std::get_if<ConstantExpression>(v.init.get()))
                init = Initial{ .i = ConvertValue(n->value, v.type) };
            else
                Abort(std::format("Non-constant initializer on local static variable '{}'", v.identifier));

            m_symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                .type = IdentifierAttributes::Static,
                .global = false,
                .init = init
            });
        } else {
            m_symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                .type = IdentifierAttributes::Local
            });

            if (v.init) {
                m_fileScope = false;
                Type init_type = std::visit(*this, *v.init);
                v.init = explicitCast(std::move(v.init), init_type, v.type);
            }
        }
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(std::monostate)
{
    return Type{ std::monostate() };
}

Error TypeChecker::CheckAndMutate(std::vector<parser::Declaration> &astVector)
{
    try {
        for (auto &i : astVector) {
            m_fileScope = true;
            std::visit(*this, i);
        }
        return Error::ALL_OK;
    } catch (const TypeError &e) {
        std::cerr << e.what() << std::endl;
        return Error::TYPE_ERROR;
    }
}

void TypeChecker::Abort(std::string_view message)
{
    throw TypeError(
        std::format("[Type error] {}", message));
}

}; // namespace parser
