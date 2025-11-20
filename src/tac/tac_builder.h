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
    Value operator()(const parser::FuncDeclStatement &f) override;
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
    Value operator()(const parser::Declaration &d) override;
    Value operator()(std::monostate) override;

    std::vector<Instruction> Convert(const std::vector<parser::BlockItem> &list);

    std::vector<Instruction> m_instructions;
};

}; // tac
