#pragma once

#include "parser/ast_visitor.h"
#include "tac_nodes.h"
#include <vector>

namespace tac {

struct TACBuilder : public parser::IASTVisitor<tac::Value> {
    Value operator()(const parser::NumberExpression &n) override;
    Value operator()(const parser::UnaryExpression &u) override;
    Value operator()(const parser::FuncDeclStatement &f) override;
    Value operator()(const parser::ReturnStatement &r) override;
    Value operator()(const parser::BlockStatement &b) override;
    Value operator()(std::monostate) override;

    std::vector<Instruction> Convert(parser::BlockStatement *b);

    std::vector<Instruction> m_instructions;
};

}; // tac
