#include "type_checker.h"

namespace parser {

struct TypeError : public std::runtime_error
{
    explicit TypeError(std::string_view msg)
    : std::runtime_error(std::string(msg))
    {
    }
};

void TypeChecker::operator()(ConstantExpression &)
{
}

void TypeChecker::operator()(VariableExpression &v)
{
    auto it = m_symbolTable->find(v.identifier);
    if (it != m_symbolTable->end() && it->second.type.getAs<FunctionType>())
        Abort(std::format("Function name '{}' is used as variable", v.identifier));
}

void TypeChecker::operator()(CastExpression &)
{
}

void TypeChecker::operator()(UnaryExpression &u)
{
    std::visit(*this, *u.expr);
}

void TypeChecker::operator()(BinaryExpression &b)
{
    std::visit(*this, *b.lhs);
    std::visit(*this, *b.rhs);
}

void TypeChecker::operator()(AssignmentExpression &a)
{
    std::visit(*this, *a.lhs);
    std::visit(*this, *a.rhs);
}

void TypeChecker::operator()(ConditionalExpression &c)
{
    std::visit(*this, *c.condition);
    std::visit(*this, *c.trueBranch);
    std::visit(*this, *c.falseBranch);
}

void TypeChecker::operator()(FunctionCallExpression &f)
{
    if (auto symbol = lookupSymbolAs<FunctionType>(f.identifier)) {
        auto &[type, attrs] = *symbol;
        if (type.params.size() != f.args.size())
            Abort(std::format("Function '{}' is called with wrong number of arguments", f.identifier));
    } else {
        // The symbol name exists, we verified it during the semantic analysis.
        Abort(std::format("'{}' is not a function name", f.identifier));
    }

    for (auto &a : f.args)
        std::visit(*this, *a);
}

void TypeChecker::operator()(ReturnStatement &r)
{
    std::visit(*this, *r.expr);
}

void TypeChecker::operator()(IfStatement &i)
{
    std::visit(*this, *i.condition);
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch)
        std::visit(*this, *i.falseBranch);
}

void TypeChecker::operator()(GotoStatement &)
{
}

void TypeChecker::operator()(LabeledStatement &l)
{
    std::visit(*this, *l.statement);
}

void TypeChecker::operator()(BlockStatement &b)
{
    for (auto &i : b.items)
        std::visit(*this, i);
}

void TypeChecker::operator()(ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
}

void TypeChecker::operator()(NullStatement &)
{
}

void TypeChecker::operator()(BreakStatement &)
{
}

void TypeChecker::operator()(ContinueStatement &)
{
}

void TypeChecker::operator()(WhileStatement &w)
{
    std::visit(*this, *w.condition);
    std::visit(*this, *w.body);
}

void TypeChecker::operator()(DoWhileStatement &d)
{
    std::visit(*this, *d.body);
    std::visit(*this, *d.condition);
}

void TypeChecker::operator()(ForStatement &f)
{
    if (f.init) {
        m_forLoopInitializer = true;
        std::visit(*this, *f.init);
        m_forLoopInitializer = false;
    }
    if (f.condition)
        std::visit(*this, *f.condition);
    if (f.update)
        std::visit(*this, *f.update);
    std::visit(*this, *f.body);
}

void TypeChecker::operator()(SwitchStatement &s)
{
    std::visit(*this, *s.condition);
    std::visit(*this, *s.body);
}

void TypeChecker::operator()(CaseStatement &c)
{
    std::visit(*this, *c.condition);
    std::visit(*this, *c.statement);
}

void TypeChecker::operator()(DefaultStatement &d)
{
    std::visit(*this, *d.statement);
}

void TypeChecker::operator()(FunctionDeclaration &f)
{
    if (!m_fileScope && f.storage == StorageStatic)
        Abort(std::format("Function '{}' can't be declared as static in block scope", f.name));

    bool already_defined = false;
    bool is_global = f.storage != StorageStatic;

    if (m_symbolTable->contains(f.name)) {
        const SymbolEntry &entry = (*m_symbolTable)[f.name];
        if (entry.type != f.type)
            Abort(std::format("Incompatible function declarations of '{}'", f.name));
        if (entry.attrs.defined && f.body)
            Abort(std::format("Function '{}' is defined more than once", f.name));
        already_defined = entry.attrs.defined;
        if (entry.attrs.global && f.storage == StorageStatic)
            Abort(std::format("Static function declaration '{}' follows non-static", f.name));
        is_global = entry.attrs.global;
    }

    insertSymbol(f.name, f.type, IdentifierAttributes{
        .type = IdentifierAttributes::Function,
        .defined = already_defined || (bool)f.body,
        .global = is_global
    });

    if (f.body) {
        m_fileScope = false;
        for (auto &p : f.params)
            insertSymbol(p, Type{ BasicType::Int }, IdentifierAttributes{
                .type = IdentifierAttributes::Local,
                .defined = false
            });
        std::visit(*this, *f.body);
    }
}

void TypeChecker::operator()(VariableDeclaration &v)
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
            init = Initial{ .i = n->value };
        else
            Abort(std::format("Non-constant initializer of '{}'", v.identifier));

        bool is_global = (v.storage != StorageStatic);

        if (m_symbolTable->contains(v.identifier)) {
            const SymbolEntry &entry = (*m_symbolTable)[v.identifier];
            if (entry.type.getAs<FunctionType>())
                Abort(std::format("Function '{}' redeclared as variable", v.identifier));

            if (v.storage == StorageExtern)
                is_global = entry.attrs.global;
            else if (entry.attrs.global != is_global)
                Abort(std::format("Conflicting variable linkage ('{}')", v.identifier));

            if (std::get_if<Initial>(&entry.attrs.init)) {
                if (std::get_if<Initial>(&init))
                    Abort(std::format("Conflicting file scope variable definition ('{}')", v.identifier));
                else
                    init = entry.attrs.init;
            } else if (!std::holds_alternative<Initial>(init) && std::holds_alternative<Tentative>(entry.attrs.init))
                init = Tentative{};
        }

        insertSymbol(v.identifier, Type{ BasicType::Int }, IdentifierAttributes{
            .type = IdentifierAttributes::Static,
            .global = is_global,
            .init = init
        });
    } else {
        // Block-level variable
        if (v.storage == StorageExtern) {
            if (v.init)
                Abort(std::format("Initializer on local extern variable '{}'", v.identifier));

            if (m_symbolTable->contains(v.identifier)) {
                const SymbolEntry &entry = (*m_symbolTable)[v.identifier];
                if (entry.type.getAs<FunctionType>())
                    Abort(std::format("Function '{}' redeclared as variable", v.identifier));
            } else {
                insertSymbol(v.identifier, Type{ BasicType::Int }, IdentifierAttributes{
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
                init = Initial{ .i = n->value };
            else
                Abort(std::format("Non-constant initializer on local static variable '{}'", v.identifier));

            insertSymbol(v.identifier, Type{ BasicType::Int }, IdentifierAttributes{
                .type = IdentifierAttributes::Static,
                .global = false,
                .init = init
            });
        } else {
            insertSymbol(v.identifier, Type{ BasicType::Int }, IdentifierAttributes{
                .type = IdentifierAttributes::Local
            });

            if (v.init) {
                m_fileScope = false;
                std::visit(*this, *v.init);
            }
        }
    }
}

void TypeChecker::operator()(std::monostate)
{
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

void TypeChecker::insertSymbol(
    const std::string &name,
    const Type &type,
    const IdentifierAttributes &attr)
{
    (*m_symbolTable)[name] = SymbolEntry{ type , attr };
}

template <typename T>
std::optional<std::pair<const T &, const IdentifierAttributes &>>
TypeChecker::lookupSymbolAs(const std::string &name)
{
    auto it = m_symbolTable->find(name);
    if (it == m_symbolTable->end())
        return std::nullopt;
    const TypeInfo &info = it->second.type.t;
    if (const T *ptr = std::get_if<T>(&info))
        return std::make_pair(std::cref(*ptr), it->second.attrs);
    return std::nullopt;
}

}; // namespace parser
