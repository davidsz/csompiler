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

void SemanticAnalyzer::enterScope()
{
    m_scopes.emplace_back();
}

void SemanticAnalyzer::leaveScope()
{
    m_scopes.pop_back();
}

SemanticAnalyzer::Scope &SemanticAnalyzer::currentScope()
{
    return m_scopes.back();
}

SemanticAnalyzer::IdentifierInfo *SemanticAnalyzer::lookupIdentifier(const std::string &name)
{
    for (auto scope = m_scopes.rbegin(); scope != m_scopes.rend(); scope++) {
        auto found = scope->find(name);
        if (found != scope->end())
            return &found->second;
    }
    return nullptr;
}

std::optional<std::string> SemanticAnalyzer::getInnermostLoopLabel()
{
    for (auto it = m_controlFlowLabels.rbegin(); it != m_controlFlowLabels.rend(); it++) {
        if (it->second == Loop)
            return it->first;
    }
    return std::nullopt;
}

std::optional<std::string> SemanticAnalyzer::getInnermostSwitchLabel()
{
    for (auto it = m_controlFlowLabels.rbegin(); it != m_controlFlowLabels.rend(); it++) {
        if (it->second == Switch)
            return it->first;
    }
    return std::nullopt;
}

std::optional<std::string> SemanticAnalyzer::getInnermostLabel()
{
    if (!m_controlFlowLabels.empty())
        return m_controlFlowLabels.back().first;
    return std::nullopt;
}

void SemanticAnalyzer::operator()(ConstantExpression &)
{
}

void SemanticAnalyzer::operator()(VariableExpression &v)
{
    if (m_currentStage > IDENTIFIER_RESOLUTION)
        return;

    if (auto info = lookupIdentifier(v.identifier))
        v.identifier = info->uniqueName;
    else
        Abort(std::format("Undeclared variable '{}'", v.identifier));
}

void SemanticAnalyzer::operator()(CastExpression &c)
{
    std::visit(*this, *c.expr);
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

void SemanticAnalyzer::operator()(ConditionalExpression &c)
{
    std::visit(*this, *c.condition);
    std::visit(*this, *c.trueBranch);
    std::visit(*this, *c.falseBranch);
}

void SemanticAnalyzer::operator()(FunctionCallExpression &f)
{
    if (m_currentStage == IDENTIFIER_RESOLUTION) {
        // In valid programs, the unique name is the same as the old name.
        // We assign it as a new name to catch errors later like the identifier
        // is being a variable name which can't be called.
        if (auto info = lookupIdentifier(f.identifier))
            f.identifier = info->uniqueName;
        else
            Abort(std::format("Undeclared function '{}' can not be called", f.identifier));
    }

    for (auto &a : f.args)
        std::visit(*this, *a);
}

void SemanticAnalyzer::operator()(ReturnStatement &r)
{
    std::visit(*this, *r.expr);
}

void SemanticAnalyzer::operator()(IfStatement &i)
{
    std::visit(*this, *i.condition);
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch)
        std::visit(*this, *i.falseBranch);
}

void SemanticAnalyzer::operator()(GotoStatement &g)
{
    // Check if we refer to declared labels
    // If yes, we replace it with its unique name
    if (m_currentStage == LABEL_ANALYSIS) {
        auto &s = m_labels[m_currentFunction];
        if (!s.contains(g.label))
            Abort(std::format("Goto refers to an undeclared label '{}' inside function '{}'", g.label, m_currentFunction));
        g.label = s[g.label];
    }
}

void SemanticAnalyzer::operator()(LabeledStatement &l)
{
    if (m_currentStage == IDENTIFIER_RESOLUTION) {
        // Collect labels for the later stages, catch duplications here
        std::string unique_name = makeNameUnique(l.label);
        auto &s = m_labels[m_currentFunction];
        if (s.contains(l.label))
            Abort(std::format("Label '{}' declared multiple times inside function '{}'", l.label, m_currentFunction));
        s.insert(std::make_pair(l.label, unique_name));
        l.label = unique_name;
    }

    std::visit(*this, *l.statement);
}

void SemanticAnalyzer::operator()(BlockStatement &b)
{
    // In function declarations, we already entered the
    // scope when processing the arguments
    bool has_own_scope = !m_parentIsAFunction;
    m_parentIsAFunction = false;

    if (has_own_scope)
        enterScope();
    for (auto &i : b.items)
        std::visit(*this, i);
    if (has_own_scope)
        leaveScope();
}

void SemanticAnalyzer::operator()(ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
}

void SemanticAnalyzer::operator()(NullStatement &)
{
}

void SemanticAnalyzer::operator()(BreakStatement &b)
{
    if (m_currentStage != LOOP_LABELING)
        return;

    // Allowed in switches and loops
    if (auto label = getInnermostLabel())
        b.label = *label;
    else
        Abort("Break is not allowed outside of switch and loops.");
}

void SemanticAnalyzer::operator()(ContinueStatement &c)
{
    if (m_currentStage != LOOP_LABELING)
        return;

    // Allowed only in loops
    if (auto label = getInnermostLoopLabel())
        c.label = *label;
    else
        Abort("Continue is not allowed outside of loops.");
}

void SemanticAnalyzer::operator()(WhileStatement &w)
{
    if (m_currentStage == LOOP_LABELING) {
        w.label = makeNameUnique("while");
        m_controlFlowLabels.push_back(make_pair(w.label, Loop));
    }

    std::visit(*this, *w.condition);
    std::visit(*this, *w.body);

    if (m_currentStage == LOOP_LABELING)
        m_controlFlowLabels.pop_back();
}

void SemanticAnalyzer::operator()(DoWhileStatement &d)
{
    if (m_currentStage == LOOP_LABELING) {
        d.label = makeNameUnique("do");
        m_controlFlowLabels.push_back(make_pair(d.label, Loop));
    }

    std::visit(*this, *d.body);
    std::visit(*this, *d.condition);

    if (m_currentStage == LOOP_LABELING)
        m_controlFlowLabels.pop_back();
}

void SemanticAnalyzer::operator()(ForStatement &f)
{
    // The for header introduces a new variable scope
    if (m_currentStage == IDENTIFIER_RESOLUTION)
        enterScope();

    if (m_currentStage == LOOP_LABELING) {
        f.label = makeNameUnique("for");
        m_controlFlowLabels.push_back(make_pair(f.label, Loop));
    }

    if (f.init)
        std::visit(*this, *f.init);
    if (f.condition)
        std::visit(*this, *f.condition);
    if (f.update)
        std::visit(*this, *f.update);
    std::visit(*this, *f.body);

    if (m_currentStage == LOOP_LABELING)
        m_controlFlowLabels.pop_back();

    if (m_currentStage == IDENTIFIER_RESOLUTION)
        leaveScope();
}

void SemanticAnalyzer::operator()(SwitchStatement &s)
{
    if (m_currentStage == LOOP_LABELING) {
        m_switches.push_back(&s);
        s.label = makeNameUnique("switch");
        m_controlFlowLabels.push_back(make_pair(s.label, Switch));
    }

    std::visit(*this, *s.condition);
    std::visit(*this, *s.body);

    if (m_currentStage == LOOP_LABELING) {
        m_controlFlowLabels.pop_back();
        m_switches.pop_back();
    }
}

void SemanticAnalyzer::operator()(CaseStatement &c)
{
    if (m_currentStage == LOOP_LABELING) {
        if (auto switch_label = getInnermostSwitchLabel()) {
            // TODO: It should be constant expression, but not necessarily a number
            if (auto expr = std::get_if<ConstantExpression>(c.condition.get())) {
                int expr_value = *std::get_if<int>(&expr->value);
                c.label = std::format("case_{}_{}", *switch_label, expr_value);
                SwitchStatement *s = m_switches.back();
                auto [it, inserted] = s->cases.insert(expr_value);
                if (!inserted)
                    Abort("Duplicate case in switch");
            } else
                Abort("Invalid expression in case statement");
        } else
            Abort("Case statement is not allowed outside of switch");
    }

    std::visit(*this, *c.condition);
    std::visit(*this, *c.statement);
}

void SemanticAnalyzer::operator()(DefaultStatement &d)
{
    if (m_currentStage == LOOP_LABELING) {
        if (auto label = getInnermostSwitchLabel())
            d.label = std::format("default_{}", *label);
        else
            Abort("Default statement is not allowed outside of switch");

        if (!m_switches.empty()) {
            SwitchStatement *s = m_switches.back();
            if (s->hasDefault)
                Abort("Duplicate default in switch");
            else
                s->hasDefault = true;
        }
    }

    std::visit(*this, *d.statement);
}

void SemanticAnalyzer::operator()(FunctionDeclaration &f)
{
    m_currentFunction = f.name;

    if (m_currentStage == IDENTIFIER_RESOLUTION) {
        if (m_scopes.size() != 1 && f.body)
            Abort(std::format("Function definition ({}) allowed only in the top level scope.", f.name));

        // Resolve the function name first
        auto it = currentScope().find(f.name);
        if (it != currentScope().end() && !it->second.hasLinkage)
            Abort(std::format("Duplicate function declaration ({})", f.name));

        currentScope()[f.name] = IdentifierInfo {
            .uniqueName = f.name,
            .hasLinkage = true
        };

        // Function arguments introduce a new variable scope
        enterScope();
        std::vector<std::string> new_params;
        // They are handled the same way as local variable declarations
        for (auto &p : f.params) {
            if (currentScope().contains(p))
                Abort(std::format("Duplicate function parameter ({})", p));
            std::string unique_name = makeNameUnique(p);
            currentScope()[p] = IdentifierInfo {
                .uniqueName = unique_name,
                .hasLinkage = false
            };
            new_params.push_back(unique_name);
        }
        // Update them to unique parameter names
        f.params = new_params;
    }

    if (f.body) {
        m_parentIsAFunction = true;
        std::visit(*this, *f.body);
    }

    if (m_currentStage == IDENTIFIER_RESOLUTION)
        leaveScope();

    m_currentFunction = "";
}

void SemanticAnalyzer::operator()(VariableDeclaration &v)
{
    if (m_currentStage == IDENTIFIER_RESOLUTION) {
        if (m_scopes.size() == 1) {
            // Top level declarations
            currentScope()[v.identifier] = IdentifierInfo {
                .uniqueName = v.identifier,
                .hasLinkage = true
            };
        } else {
            // Block level variables
            // Extern declaration conflicts within the same scope
            auto prev = currentScope().find(v.identifier);
            if (prev != currentScope().end()) {
                if (!prev->second.hasLinkage || v.storage != StorageExtern)
                    Abort(std::format("Conflicting local declaration ({})", v.identifier));
            }

            if (v.storage == StorageExtern) {
                // Don't rename extern variables
                currentScope()[v.identifier] = IdentifierInfo{
                    .uniqueName = v.identifier,
                    .hasLinkage = true
                };
            } else {
                // Give variables globally unique names; different variables
                // can have the same names in different scopes
                std::string unique_name = makeNameUnique(v.identifier);
                currentScope()[v.identifier] = IdentifierInfo{
                    .uniqueName = unique_name,
                    .hasLinkage = false
                };
                v.identifier = unique_name;
            }
        }
    }

    if (v.init)
        std::visit(*this, *v.init);
}

void SemanticAnalyzer::operator()(std::monostate)
{
}

Error SemanticAnalyzer::CheckAndMutate(std::vector<parser::Declaration> &astVector)
{
    m_scopes.clear();
    enterScope();
    m_labels.clear();
    m_controlFlowLabels.clear();

    try {
        m_currentStage = IDENTIFIER_RESOLUTION;
        for (auto &i : astVector)
            std::visit(*this, i);

        m_currentStage = LABEL_ANALYSIS;
        for (auto &i : astVector)
            std::visit(*this, i);

        m_currentStage = LOOP_LABELING;
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
    throw SemanticError(
        std::format("[Semantic error in stage {}] {}", (int)m_currentStage, message));
}

} // namespace parser
