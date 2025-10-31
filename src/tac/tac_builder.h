#ifndef TAC_BUILDER_H
#define TAC_BUILDER_H

#include "parser/ast_visitor.h"
#include "tac_nodes.h"

namespace tac {

struct TACBuilder : public parser::IASTVisitor<Any> {
    Any operator()(const parser::NumberExpression &) override {
        return Empty{};
    }
    Any operator()(const parser::FuncDeclStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::ReturnStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::BlockStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::Empty &) override {
        return Empty{};
    }

    FunctionDefinition Convert(parser::BlockStatement *) {
        return FunctionDefinition{};
    }
};

}; // tac

#endif // TAC_BUILDER_H
