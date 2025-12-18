#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"
#include "common/symbol_table.h"

namespace parser {

struct TypeChecker : public IASTMutatingVisitor<Type> {
    Type operator()(ConstantExpression &n) override;
    Type operator()(VariableExpression &v) override;
    Type operator()(CastExpression &v) override;
    Type operator()(UnaryExpression &u) override;
    Type operator()(BinaryExpression &b) override;
    Type operator()(AssignmentExpression &a) override;
    Type operator()(ConditionalExpression &c) override;
    Type operator()(FunctionCallExpression &f) override;
    Type operator()(ReturnStatement &r) override;
    Type operator()(IfStatement &i) override;
    Type operator()(GotoStatement &g) override;
    Type operator()(LabeledStatement &l) override;
    Type operator()(BlockStatement &b) override;
    Type operator()(ExpressionStatement &e) override;
    Type operator()(NullStatement &e) override;
    Type operator()(BreakStatement &b) override;
    Type operator()(ContinueStatement &c) override;
    Type operator()(WhileStatement &w) override;
    Type operator()(DoWhileStatement &d) override;
    Type operator()(ForStatement &f) override;
    Type operator()(SwitchStatement &s) override;
    Type operator()(CaseStatement &c) override;
    Type operator()(DefaultStatement &d) override;
    Type operator()(FunctionDeclaration &f) override;
    Type operator()(VariableDeclaration &v) override;
    Type operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::Declaration> &);
    void Abort(std::string_view);

    std::shared_ptr<SymbolTable> symbolTable() { return m_symbolTable; }

private:
    std::shared_ptr<SymbolTable> m_symbolTable = std::make_shared<SymbolTable>();

    bool m_fileScope = false;
    bool m_forLoopInitializer = false;
    std::vector<Type> m_functionTypeStack;
    std::vector<SwitchStatement *> m_switches;
};

}; // namespace parser
