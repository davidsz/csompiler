#ifndef TAC_BUILDER_H
#define TAC_BUILDER_H

#include "parser/ast_visitor.h"
#include "tac_nodes.h"

namespace tac {

struct TACBuilder : public parser::IASTVisitor<Any> {
    // --- Expressions ---
    Any operator()(const parser::NumberExpression &) override {
        return Empty{};
    }
    Any operator()(const parser::VariableExpression &) override {
        return Empty{};
    }
    Any operator()(const parser::BinaryExpression &) override {
        return Empty{};
    }
    Any operator()(const parser::CallExpression &) override {
        return Empty{};
    }

    // --- Statements ---
    Any operator()(const parser::VarDeclStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::FuncDeclStatement &f) override {
        auto func = FunctionDef{};
        func.name = f.name;
        // func.instructions = unwrap<InstructionList>(std::visit(*this, *f.body));
        return std::move(func);
    }
    Any operator()(const parser::ExpressionStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::ReturnStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::IfStatement &) override {
        return Empty{};
    }
    Any operator()(const parser::BlockStatement &) override {
        return Empty{};
    }

    void Convert(parser::BlockStatement *s) {
        for (auto &stmt : s->statements)
            std::visit(*this, *stmt);
    }
};

}; // tac

#endif // TAC_BUILDER_H
