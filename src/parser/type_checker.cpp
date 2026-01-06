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
    if (first == second)
        return first;
    if (first.isBasic(Double) || second.isBasic(Double))
        return Type{ BasicType::Double };
    if (first.size() == second.size())
        return first.isSigned() ? second : first;
    return first.size() > second.size() ? first : second;
}

// It must be a constant literal, it must be
// an integer, and its value must be 0.
static bool isNullPointerConstant(const Expression &expr)
{
    if (const ConstantExpression *c = std::get_if<ConstantExpression>(&expr)) {
        return std::visit([](const auto &x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr(std::is_floating_point_v<T>)
                return false;
            if constexpr (std::is_integral_v<T>)
                return x == 0;
            else
                return false;
        }, c->value);
    }
    return false;
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

static std::unique_ptr<Expression> convertByAssignment(
    std::unique_ptr<Expression> expr,
    const Type &from_type,
    const Type &to_type)
{
    assert(from_type.isInitialized() && to_type.isInitialized());
    if (from_type == to_type)
        return expr;
    if (from_type.isArithmetic() && to_type.isArithmetic())
        return explicitCast(std::move(expr), from_type, to_type);
    if (isNullPointerConstant(*expr) && to_type.isPointer())
        return explicitCast(std::move(expr), from_type, to_type);
    return nullptr;
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

static bool isLvalue(const Expression &expr)
{
    return std::holds_alternative<VariableExpression>(expr)
        || std::holds_alternative<DereferenceExpression>(expr);
}

static std::optional<Type> getCommonPointerType(
    const Expression &first_expr,
    const Type &first_type,
    const Expression &second_expr,
    const Type &second_type)
{
    assert(first_type.isInitialized() && second_type.isInitialized());
    if (first_type == second_type)
        return first_type;
    if (isNullPointerConstant(first_expr))
        return second_type;
    if (isNullPointerConstant(second_expr))
        return first_type;
    return std::nullopt;
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
    if ((c.inner_type.isPointer() && c.target_type.isBasic(Double))
        || (c.inner_type.isBasic(Double) && c.target_type.isPointer()))
        Abort("Not allowed to cast pointer from/to a floating point type.");
    return c.target_type;
}

Type TypeChecker::operator()(UnaryExpression &u)
{
    // canBePostfix also covers the prefix versions of ++ and --
    if (canBePostfix(u.op) && !isLvalue(*u.expr))
        Abort("Invalid lvalue in unary expression");

    Type type = std::visit(*this, *u.expr);

    if (type.isBasic(Double) && u.op == UnaryOperator::BitwiseComplement)
        Abort("The type of a unary bitwise complement operation can't be double.");
    if (type.isPointer() && u.op == UnaryOperator::BitwiseComplement)
        Abort("Can't apply complement on a pointer type.");
    if (type.isPointer() && u.op == UnaryOperator::Negate)
        Abort("Can't negate a pointer type.");

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
            || b.op == BinaryOperator::BitwiseOr
            || b.op == BinaryOperator::AssignLShift
            || b.op == BinaryOperator::AssignMod
            || b.op == BinaryOperator::AssignRShift
            || b.op == BinaryOperator::AssignBitwiseAnd
            || b.op == BinaryOperator::AssignBitwiseXor
            || b.op == BinaryOperator::AssignBitwiseOr)
            Abort("The type of the binary operation can't be double.");
    }

    if (left_type.isPointer() || right_type.isPointer()) {
        if (b.op == BinaryOperator::Multiply
            || b.op == BinaryOperator::Divide
            || b.op == BinaryOperator::Remainder
            || b.op == BinaryOperator::BitwiseAnd
            || b.op == BinaryOperator::BitwiseXor
            || b.op == BinaryOperator::BitwiseOr)
        Abort("The type of the binary operation can't be a pointer.");
    }

    if (b.op == LeftShift || b.op == RightShift) {
        if (left_type.isPointer() || right_type.isPointer())
            Abort("Right operand of bitshifts can't be a pointer.");
        // The right operand of shift operators need an integer promotion
        b.rhs = explicitCast(std::move(b.rhs), right_type, Type{ BasicType::Int });
        b.type = left_type;
        return b.type;
    }

    Type common_type;
    if (b.op == BinaryOperator::Equal || b.op == BinaryOperator::NotEqual) {
        if (left_type.isPointer() || right_type.isPointer()) {
            if (auto cpt = getCommonPointerType(*b.lhs, left_type, *b.rhs, right_type))
                common_type = *cpt;
            else
                Abort("Expressions have incompatible pointer types");
        } else
            common_type = getCommonType(left_type, right_type);
    } else
        common_type = getCommonType(left_type, right_type);

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
    if (!isLvalue(*a.lhs))
        Abort("The left side of an assignment should be an lvalue.");
    Type left_type = std::visit(*this, *a.lhs);
    Type right_type = std::visit(*this, *a.rhs);
    if (!(a.rhs = convertByAssignment(std::move(a.rhs), right_type, left_type)))
        Abort("Can't convert type for assignment");
    a.type = left_type;
    return a.type;
}

Type TypeChecker::operator()(CompoundAssignmentExpression &c)
{
    if (!isLvalue(*c.lhs))
        Abort("The left side of a compound assignment should be an lvalue.");
    Type left_type = std::visit(*this, *c.lhs);
    Type right_type = std::visit(*this, *c.rhs);

    if (left_type.isBasic(Double) || right_type.isBasic(Double)) {
        if (c.op == BinaryOperator::AssignLShift
            || c.op == BinaryOperator::AssignMod
            || c.op == BinaryOperator::AssignRShift
            || c.op == BinaryOperator::AssignBitwiseAnd
            || c.op == BinaryOperator::AssignBitwiseXor
            || c.op == BinaryOperator::AssignBitwiseOr)
            Abort("The type of the compound operation can't be double.");
    }

    if (left_type.isPointer() || right_type.isPointer()) {
        if (c.op == BinaryOperator::AssignMult
            || c.op == BinaryOperator::AssignDiv
            || c.op == BinaryOperator::AssignMod
            || c.op == BinaryOperator::AssignBitwiseAnd
            || c.op == BinaryOperator::AssignBitwiseXor
            || c.op == BinaryOperator::AssignBitwiseOr)
            Abort("The type of the binary operation can't be a pointer.");
    }

    if (c.op == BinaryOperator::AssignLShift || c.op == BinaryOperator::AssignRShift) {
        // The right operand of shift operators need an integer promotion
        c.rhs = explicitCast(std::move(c.rhs), right_type, Type{ BasicType::Int });
        c.inner_type = left_type;
        c.result_type = left_type;
        return c.result_type;
    }

    Type common_type = getCommonType(left_type, right_type);
    c.lhs = explicitCast(std::move(c.lhs), left_type, common_type);
    c.rhs = explicitCast(std::move(c.rhs), right_type, common_type);
    c.inner_type = common_type;
    c.result_type = left_type;
    return c.result_type;
}

Type TypeChecker::operator()(ConditionalExpression &c)
{
    Type condition_type = std::visit(*this, *c.condition);
    if (condition_type.isBasic(Double))
        c.condition = notNot(std::move(c.condition));
    Type true_type = std::visit(*this, *c.trueBranch);
    Type false_type = std::visit(*this, *c.falseBranch);

    Type common_type;
    if (true_type.isPointer() || false_type.isPointer()) {
        if (auto cpt = getCommonPointerType(*c.trueBranch, true_type, *c.falseBranch, false_type))
            common_type = *cpt;
        else
            Abort("Expressions have incompatible pointer types");
    } else
        common_type = getCommonType(true_type, false_type);

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
            if (!(f.args[i] = convertByAssignment(std::move(f.args[i]), arg_type, *type->params[i])))
                Abort("Can't convert argument type for function call");
        }
        f.type = type->ret;
        return *f.type;
    } else {
        // The symbol name exists, we verified it during the semantic analysis.
        Abort(std::format("'{}' is not a function name", f.identifier));
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DereferenceExpression &d)
{
    Type type = std::visit(*this, *d.expr);
    if (PointerType *pointer_type = type.getAs<PointerType>())
        d.type = *pointer_type->referenced;
    else
        Abort("Can't dereference a non-pointer");
    return d.type;
}

Type TypeChecker::operator()(AddressOfExpression &a)
{
    if (!isLvalue(*a.expr))
        Abort("Can't take the address of a non-lvalue");
    Type type = std::visit(*this, *a.expr);
    a.type = Type{ PointerType{ .referenced = std::make_shared<Type>(type) } };
    return a.type;
}

Type TypeChecker::operator()(ReturnStatement &r)
{
    Type ret_type = std::visit(*this, *r.expr);
    r.expr = convertByAssignment(
        std::move(r.expr),
        ret_type,
        *std::get<FunctionType>(m_functionTypeStack.back().t).ret);
    if (!r.expr)
        Abort("Can't convert return type");
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

    if (s.type.isBasic(Double) || s.type.isPointer())
        Abort("The type of a switch statement has to be az integer.");

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
        } else if (v.type.isPointer()) {
            if (!isNullPointerConstant(*v.init))
                Abort(std::format("Trying to initialize pointer '{}' with a non-null constant", v.identifier));
            init = Initial{ .i = MakeConstantValue(0, ULong) };
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
                init = Initial{ .i = MakeConstantValue(0, v.type) };
            else if (v.type.isPointer()) {
                if (!isNullPointerConstant(*v.init))
                    Abort(std::format("Trying to initialize pointer '{}' with a non-null constant", v.identifier));
                init = Initial{ .i = MakeConstantValue(0, ULong) };
            } else if (auto n = std::get_if<ConstantExpression>(v.init.get()))
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
                if (v.type.isPointer()) {
                    if (std::holds_alternative<ConstantExpression>(*v.init) && !isNullPointerConstant(*v.init))
                        Abort("A non-null constant can't initialize a pointer type.");
                }
                m_fileScope = false;
                Type init_type = std::visit(*this, *v.init);
                if (!(v.init = convertByAssignment(std::move(v.init), init_type, v.type)))
                    Abort("Can't convert initializer type.");
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
