#ifndef ASM_BUILDER_H
#define ASM_BUILDER_H

#include "asm_nodes.h"
#include "tac/tac_visitor.h"

namespace assembly {

struct ASMBuilder : public tac::ITACVisitor<Any> {
    Any operator()(const tac::FunctionDef &) override {
        auto func = FuncDecl{};
        // func.name = f.name;
        // func.instructions = unwrap<InstructionList>(std::visit(*this, *f.body));
        return std::move(func);
    }

    Any operator()(const tac::Return &) override { return Ret{}; }
    Any operator()(const tac::Unary &) override { return Ret{}; }

    Any operator()(const tac::Constant &) override { return Ret{}; }
    Any operator()(const tac::Identifier &) override { return Ret{}; }

    Any operator()(const tac::Empty &) override { return Ret{}; }

    void Convert(const tac::FunctionDef *) {
        /*
        for (auto &stmt : s->statements)
            std::visit(*this, *stmt);
        */
    }
};

}; // assembly

#endif // ASM_BUILDER_H
