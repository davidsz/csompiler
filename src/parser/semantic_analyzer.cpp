#include "semantic_analyzer.h"
#include <variant>

namespace parser {

struct SemanticError : public std::runtime_error
{
    explicit SemanticError(std::string_view msg)
    : std::runtime_error(std::string(msg))
    {
    }
};

static std::string makeNameUnique(std::string_view name)
{
    // Generate a name which is not valid in the C syntax,
    // this avoids collision with function names, etc.
    static size_t counter = 0;
    return std::format("{}.{}", name, counter++);
}

void SemanticAnalyzer::operator()(NumberExpression &)
{
}

void SemanticAnalyzer::operator()(VariableExpression &v)
{
    auto it = m_variables.find(v.identifier);
    if (it != m_variables.end())
        v.identifier = it->second;
    else
        Abort("Undeclared variable");
}

void SemanticAnalyzer::operator()(UnaryExpression &u)
{
    // canBePostfix also covers the prefix versions of ++ and --
    if (canBePostfix(u.op) && !std::holds_alternative<VariableExpression>(*u.expr))
        Abort("Invalid lvalue in unary expression");
    std::visit(*this, *u.expr);
}

void SemanticAnalyzer::operator()(BinaryExpression &b)
{
    // TODO: Figure out how to report error messages with line numbers
    if (isCompoundAssignment(b.op) && !std::holds_alternative<VariableExpression>(*b.lhs))
        Abort("Invalid lvalue in compound assignment");
    std::visit(*this, *b.lhs);
    std::visit(*this, *b.rhs);
}

void SemanticAnalyzer::operator()(AssignmentExpression &a)
{
    // TODO: Figure out how to report error messages with line numbers
    if (!std::holds_alternative<VariableExpression>(*a.lhs))
        Abort("Invalid lvalue in assignment");
    std::visit(*this, *a.lhs);
    std::visit(*this, *a.rhs);
}

void SemanticAnalyzer::operator()(FuncDeclStatement &f)
{
    for (auto &i : f.body)
        std::visit(*this, i);
}

void SemanticAnalyzer::operator()(ReturnStatement &r)
{
    std::visit(*this, *r.expr);
}

void SemanticAnalyzer::operator()(BlockStatement &)
{
}

void SemanticAnalyzer::operator()(ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
}

void SemanticAnalyzer::operator()(NullStatement &)
{
}

void SemanticAnalyzer::operator()(Declaration &d)
{
    // Prohibit duplicate variabe declarations
    if (m_variables.contains(d.identifier))
        Abort(std::format("Duplicate variable declaration ({})", d.identifier));

    // Give variables globally unique names; this will help in later stages
    // when different variables can have the same names in different scopes
    std::string unique_name = makeNameUnique(d.identifier);
    m_variables.insert({d.identifier, unique_name});
    d.identifier = unique_name;
    if (d.init)
        std::visit(*this, *d.init);
}

void SemanticAnalyzer::operator()(std::monostate)
{
}

Error SemanticAnalyzer::CheckAndMutate(std::vector<parser::BlockItem> &astVector)
{
    m_variables.clear();
    try {
        for (auto &i : astVector)
            std::visit(*this, i);
        return Error::ALL_OK;
    } catch (const SemanticError &e) {
        std::cerr << e.what() << std::endl;
        return Error::SEMANTIC_ERROR;
    }
}

void SemanticAnalyzer::Abort(std::string_view message)
{
    throw SemanticError(message);
}

} // namespace parser
