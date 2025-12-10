#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"

namespace parser {

struct TypeChecker : public IASTMutatingVisitor<void> {
    void operator()(ConstantExpression &n) override;
    void operator()(VariableExpression &v) override;
    void operator()(CastExpression &v) override;
    void operator()(UnaryExpression &u) override;
    void operator()(BinaryExpression &b) override;
    void operator()(AssignmentExpression &a) override;
    void operator()(ConditionalExpression &c) override;
    void operator()(FunctionCallExpression &f) override;
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
    void operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::Declaration> &);
    void Abort(std::string_view);

    std::shared_ptr<SymbolTable> symbolTable() { return m_symbolTable; }

private:
    std::shared_ptr<SymbolTable> m_symbolTable = std::make_shared<SymbolTable>();
    void insertSymbol(const std::string &name, const Type &, const IdentifierAttributes &);

    template <typename T>
    std::optional<std::pair<const T &, const IdentifierAttributes &>>
    lookupSymbolAs(const std::string &name);

    bool m_fileScope = false;
    bool m_forLoopInitializer = false;
};

}; // namespace parser
