#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"
#include <unordered_map>
#include <unordered_set>

namespace parser {

struct SemanticAnalyzer : public IASTMutatingVisitor<void> {
    enum Stage {
        // Resolves variables, function names,
        // collects symbols for later stages
        IDENTIFIER_RESOLUTION = 0,
        // Uses previously collected label names to catch errors
        LABEL_ANALYSIS = 1,
        // Label control flow statements and connect breaks/continues
        // to corresponding ones
        LOOP_LABELING = 2,
    };

    void operator()(ConstantExpression &n) override;
    void operator()(StringExpression &s) override;
    void operator()(VariableExpression &v) override;
    void operator()(CastExpression &c) override;
    void operator()(UnaryExpression &u) override;
    void operator()(BinaryExpression &b) override;
    void operator()(AssignmentExpression &a) override;
    void operator()(CompoundAssignmentExpression &c) override;
    void operator()(ConditionalExpression &c) override;
    void operator()(FunctionCallExpression &f) override;
    void operator()(DereferenceExpression &d) override;
    void operator()(AddressOfExpression &a) override;
    void operator()(SubscriptExpression &s) override;
    void operator()(SizeOfExpression &s) override;
    void operator()(SizeOfTypeExpression &s) override;
    void operator()(DotExpression &d) override;
    void operator()(ArrowExpression &a) override;
    void operator()(ReturnStatement &r) override;
    void operator()(IfStatement &i) override;
    void operator()(GotoStatement &g) override;
    void operator()(LabeledStatement &l) override;
    void operator()(BlockStatement &b) override;
    void operator()(ExpressionStatement &e) override;
    void operator()(NullStatement &e) override;
    void operator()(BreakStatement &b) override;
    void operator()(ContinueStatement &c) override;
    void operator()(WhileStatement &w) override;
    void operator()(DoWhileStatement &d) override;
    void operator()(ForStatement &f) override;
    void operator()(SwitchStatement &s) override;
    void operator()(CaseStatement &c) override;
    void operator()(DefaultStatement &d) override;
    void operator()(FunctionDeclaration &f) override;
    void operator()(VariableDeclaration &v) override;
    void operator()(StructDeclaration &s) override;
    void operator()(SingleInit &s) override;
    void operator()(CompoundInit &c) override;
    void operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::Declaration> &);
    void Abort(std::string_view);

private:
    Stage m_currentStage = IDENTIFIER_RESOLUTION;

    void enterScope();
    void leaveScope();

    // Variable / function name scopes
    struct IdentifierInfo {
        std::string uniqueName;
        bool hasLinkage;
    };
    using Scope = std::unordered_map<std::string, IdentifierInfo>;
    std::vector<Scope> m_variableFunctionScopes;
    Scope &currentScope();
    IdentifierInfo *lookupIdentifier(const std::string &name);

    // Structure tags have different scopes, because they represent types
    using StructTagScope = std::unordered_map<std::string, std::string>;
    std::vector<StructTagScope> m_structureTagScopes;
    StructTagScope &currentStructTagScope();
    std::optional<std::string> lookupStructTag(const std::string &name);

    // Not the same as TypeChecker::ValidateTypeSpecifier(),
    // this resolves structure tags
    void ValidateTypeSpecifier(Type &type);

    std::string m_currentFunction = "";
    bool m_parentIsAFunction = false;

    // Function names mapped to the labels (original name, unique name) defined inside
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_labels;

    // Labeling loops and switches
    enum ControlFlowType {
        Loop,
        Switch
    };
    std::vector<std::pair<std::string, ControlFlowType>> m_controlFlowLabels;
    std::optional<std::string> getInnermostLoopLabel();
    std::optional<std::string> getInnermostSwitchLabel();
    std::optional<std::string> getInnermostLabel();

    // Stack of switches
    std::vector<SwitchStatement *> m_switches;
};

} // namespace parser
