#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"
#include <unordered_map>
#include <unordered_set>

namespace parser {

struct SemanticAnalyzer : public IASTMutatingVisitor<void> {
    enum Stage {
        // Resolves variables, collects symbols for later stages
        VARIABLE_RESOLUTION = 0,
        // Uses previously collected label names to catch errors
        LABEL_ANALYSIS = 1,
    };

    void operator()(NumberExpression &n) override;
    void operator()(VariableExpression &v) override;
    void operator()(UnaryExpression &u) override;
    void operator()(BinaryExpression &b) override;
    void operator()(AssignmentExpression &a) override;
    void operator()(ConditionalExpression &c) override;
    void operator()(FuncDeclStatement &f) override;
    void operator()(ReturnStatement &r) override;
    void operator()(IfStatement &i) override;
    void operator()(GotoStatement &g) override;
    void operator()(LabeledStatement &l) override;
    void operator()(BlockStatement &b) override;
    void operator()(ExpressionStatement &e) override;
    void operator()(NullStatement &e) override;
    void operator()(Declaration &d) override;
    void operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::BlockItem> &);
    void Abort(std::string_view);

private:
    // Variable names mapped to their generated unique names
    using Scope = std::unordered_map<std::string, std::string>;
    std::vector<Scope> m_scopes;
    void enterScope();
    void leaveScope();
    Scope &currentScope();
    bool isDeclaredInCurrentScope(const std::string &name);
    std::optional<std::string> lookupVariable(const std::string &name);

    Stage m_currentStage = VARIABLE_RESOLUTION;
    std::string m_currentFunction = "";

    // Function names mapped to the labels defined inside
    std::unordered_map<std::string, std::unordered_set<std::string>> m_labels;
};

} // namespace parser
