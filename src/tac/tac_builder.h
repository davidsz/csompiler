#ifndef TAC_BUILDER_H
#define TAC_BUILDER_H

#include "parser/ast_visitor.h"
#include "tac_nodes.h"

namespace tac {

struct TACBuilder : public parser::IASTVisitor<tac::Value> {
    Value operator()(const parser::NumberExpression &n) override {
        return Constant{ static_cast<int>(n.value) };
    }
    Value operator()(const parser::UnaryExpression &) override {
        return Constant{ 0 };
    }
    Value operator()(const parser::FuncDeclStatement &) override {
        return Empty{};
    }
    Value operator()(const parser::ReturnStatement &) override {
        return Empty{};
    }
    Value operator()(const parser::BlockStatement &) override {
        return Empty{};
    }
    Value operator()(const parser::Empty &) override {
        return Empty{};
    }

    FunctionDefinition Convert(parser::BlockStatement *) {
        auto f = FunctionDefinition{};
        return f;
    }
};

}; // tac

#endif // TAC_BUILDER_H
