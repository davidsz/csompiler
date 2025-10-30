#ifndef ASM_BUILDER_H
#define ASM_BUILDER_H

#include "asm_nodes.h"
#include "parser/ast_visitor.h"

namespace assembly {

struct ASMBuilder : public parser::IASTVisitor<Any> {
    // --- Expressions ---
    Any operator()(const parser::NumberExpression &) override { return Ret{}; }
    Any operator()(const parser::VariableExpression &) override { return Ret{}; }
    Any operator()(const parser::BinaryExpression &) override { return Ret{}; }
    Any operator()(const parser::CallExpression &) override { return Ret{}; }

    // --- Statements ---
    Any operator()(const parser::VarDeclStatement &) override { return Ret{}; }
    Any operator()(const parser::FuncDeclStatement &f) override {
        auto func = FuncDecl{};
        func.name = f.name;
        func.instructions = unwrap<InstructionList>(std::visit(*this, *f.body));
        return std::move(func);
    }
    Any operator()(const parser::ExpressionStatement &) override { return Ret{}; }
    Any operator()(const parser::ReturnStatement &) override { return Ret{}; }
    Any operator()(const parser::IfStatement &) override { return Ret{}; }
    Any operator()(const parser::BlockStatement &) override { return InstructionList{}; }

    void Convert(parser::BlockStatement *s) {
        for (auto &stmt : s->statements)
            std::visit(*this, *stmt);
    }
};

}; // assembly

#endif // ASM_BUILDER_H
