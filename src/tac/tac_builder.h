#pragma once

#include "parser/ast_visitor.h"
#include "tac_nodes.h"
#include <vector>

namespace tac {

struct TACBuilder : public parser::IASTVisitor<tac::Value> {
    Value operator()(const parser::NumberExpression &n) override;
    Value operator()(const parser::VariableExpression &v) override;
    Value operator()(const parser::UnaryExpression &u) override;
    Value operator()(const parser::BinaryExpression &b) override;
    Value operator()(const parser::AssignmentExpression &a) override;
    Value operator()(const parser::ConditionalExpression &c) override;
    Value operator()(const parser::FunctionCallExpression &f) override;
    Value operator()(const parser::ReturnStatement &r) override;
    Value operator()(const parser::IfStatement &i) override;
    Value operator()(const parser::GotoStatement &g) override;
    Value operator()(const parser::LabeledStatement &l) override;
    Value operator()(const parser::BlockStatement &b) override;
    Value operator()(const parser::ExpressionStatement &e) override;
    Value operator()(const parser::NullStatement &e) override;
    Value operator()(const parser::BreakStatement &b) override;
    Value operator()(const parser::ContinueStatement &c) override;
    Value operator()(const parser::WhileStatement &w) override;
    Value operator()(const parser::DoWhileStatement &d) override;
    Value operator()(const parser::ForStatement &f) override;
    Value operator()(const parser::SwitchStatement &s) override;
    Value operator()(const parser::CaseStatement &c) override;
    Value operator()(const parser::DefaultStatement &d) override;
    Value operator()(const parser::FunctionDeclaration &f) override;
    Value operator()(const parser::VariableDeclaration &v) override;
    Value operator()(std::monostate) override;

    TACBuilder(std::shared_ptr<SymbolTable> symbolTable);

    std::vector<TopLevel> ConvertTopLevel(const std::vector<parser::Declaration> &list);
    std::vector<Instruction> ConvertBlock(const std::vector<parser::BlockItem> &list);
    void ProcessStaticSymbols();

    std::vector<TopLevel> m_topLevel;
    std::vector<Instruction> m_instructions;
    std::shared_ptr<SymbolTable> m_symbolTable;
};

}; // tac
