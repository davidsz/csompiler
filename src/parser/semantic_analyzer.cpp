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
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    auto it = m_variables.find(v.identifier);
    if (it != m_variables.end())
        v.identifier = it->second;
    else
        Abort("Undeclared variable");
}

void SemanticAnalyzer::operator()(UnaryExpression &u)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    // canBePostfix also covers the prefix versions of ++ and --
    if (canBePostfix(u.op) && !std::holds_alternative<VariableExpression>(*u.expr))
        Abort("Invalid lvalue in unary expression");
    std::visit(*this, *u.expr);
}

void SemanticAnalyzer::operator()(BinaryExpression &b)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    // TODO: Figure out how to report error messages with line numbers
    if (isCompoundAssignment(b.op) && !std::holds_alternative<VariableExpression>(*b.lhs))
        Abort("Invalid lvalue in compound assignment");
    std::visit(*this, *b.lhs);
    std::visit(*this, *b.rhs);
}

void SemanticAnalyzer::operator()(AssignmentExpression &a)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    // TODO: Figure out how to report error messages with line numbers
    if (!std::holds_alternative<VariableExpression>(*a.lhs))
        Abort("Invalid lvalue in assignment");
    std::visit(*this, *a.lhs);
    std::visit(*this, *a.rhs);
}

void SemanticAnalyzer::operator()(ConditionalExpression &c)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;
    std::visit(*this, *c.condition);
    std::visit(*this, *c.trueBranch);
    std::visit(*this, *c.falseBranch);
}

void SemanticAnalyzer::operator()(FuncDeclStatement &f)
{
    m_currentFunction = f.name;
    std::visit(*this, *f.body);
    m_currentFunction = "";
}

void SemanticAnalyzer::operator()(ReturnStatement &r)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    std::visit(*this, *r.expr);
}

void SemanticAnalyzer::operator()(IfStatement &i)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

    std::visit(*this, *i.condition);
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch)
        std::visit(*this, *i.falseBranch);
}

void SemanticAnalyzer::operator()(GotoStatement &g)
{
    // Check if we refer to declared labels
    if (m_currentStage == LABEL_ANALYSIS) {
        auto &s = m_labels[m_currentFunction];
        if (!s.contains(g.label))
            Abort(std::format("Goto refers to an undeclared label {} inside function {}", g.label, m_currentFunction));
    }
}

void SemanticAnalyzer::operator()(LabeledStatement &l)
{
    if (m_currentStage == VARIABLE_RESOLUTION) {
        // Collect labels for the later stages, catch duplications here
        auto &s = m_labels[m_currentFunction];
        auto [it, inserted] = s.insert(l.label);
        if (!inserted)
            Abort(std::format("Label {} declared multiple times inside function {}", l.label, m_currentFunction));

        std::visit(*this, *l.statement);
    }
}

void SemanticAnalyzer::operator()(BlockStatement &b)
{
    for (auto &i : b.items)
        std::visit(*this, i);
}

void SemanticAnalyzer::operator()(ExpressionStatement &e)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;
    std::visit(*this, *e.expr);
}

void SemanticAnalyzer::operator()(NullStatement &)
{
}

void SemanticAnalyzer::operator()(Declaration &d)
{
    if (m_currentStage > VARIABLE_RESOLUTION)
        return;

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
        m_currentStage = VARIABLE_RESOLUTION;
        for (auto &i : astVector)
            std::visit(*this, i);

        m_currentStage = LABEL_ANALYSIS;
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
