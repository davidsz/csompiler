#include "type_checker.h"

namespace parser {

struct TypeError : public std::runtime_error
{
    explicit TypeError(std::string_view msg)
    : std::runtime_error(std::string(msg))
    {
    }
};

void TypeChecker::operator()(NumberExpression &)
{
}

void TypeChecker::operator()(VariableExpression &v)
{
    auto it = m_symbolTable.find(v.identifier);
    if (it != m_symbolTable.end() && std::holds_alternative<FunType>(it->second.first))
        Abort(std::format("Function name '{}' is used as variable", v.identifier));
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
    if (auto symbol = lookupSymbolAs<FunType>(f.identifier)) {
        auto &[type, defined] = *symbol;
        if (type.paramCount != f.args.size())
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
    if (f.init)
        std::visit(*this, *f.init);
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
    auto function_type = FunType { f.params.size() };
    if (auto symbol = lookupSymbolAs<FunType>(f.name)) {
        auto &[type, defined] = *symbol;
        if (type != function_type)
            Abort(std::format("Incompatible function declarations of '{}'", f.name));
        if (defined && f.body)
            Abort(std::format("Function '{}' is defined more than once", f.name));
    }
    insertSymbol(f.name, function_type, (bool)f.body);

    if (f.body) {
        for (auto &p : f.params)
            insertSymbol(p, Int{}, false);
        std::visit(*this, *f.body);
    }
}

void TypeChecker::operator()(VariableDeclaration &v)
{
    insertSymbol(v.identifier, Int{}, (bool)v.init);
    if (v.init)
        std::visit(*this, *v.init);
}

void TypeChecker::operator()(std::monostate)
{
}

Error TypeChecker::CheckAndMutate(std::vector<parser::Declaration> &astVector)
{
    try {
        for (auto &i : astVector)
            std::visit(*this, i);
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

void TypeChecker::insertSymbol(const std::string &name, const TypeInfo &type, bool defined)
{
    m_symbolTable.insert(std::make_pair(name, std::make_pair(type, defined)));
}

template <typename T>
std::optional<std::pair<const T &, bool>> TypeChecker::lookupSymbolAs(const std::string &name)
{
    auto it = m_symbolTable.find(name);
    if (it == m_symbolTable.end())
        return std::nullopt;
    const TypeInfo &info = it->second.first;
    if (const T *ptr = std::get_if<T>(&info))
        return std::make_pair(std::cref(*ptr), it->second.second);
    return std::nullopt;
}

}; // namespace parser
