#pragma once

#include "asm_nodes.h"
#include "tac/tac_visitor.h"

namespace assembly {

struct ASMBuilder : public tac::ITACVisitor<Operand> {
    Operand operator()(const tac::FunctionDefinition &) override;
    Operand operator()(const tac::Return &) override;
    Operand operator()(const tac::Unary &) override;
    Operand operator()(const tac::Binary &) override;
    Operand operator()(const tac::Copy &) override;
    Operand operator()(const tac::Jump &) override;
    Operand operator()(const tac::JumpIfZero &) override;
    Operand operator()(const tac::JumpIfNotZero &) override;
    Operand operator()(const tac::Label &) override;
    Operand operator()(const tac::Constant &) override;
    Operand operator()(const tac::Variant &) override;
    Operand operator()(std::monostate) override {
        assert(false);
        return std::monostate();
    }

    std::list<Instruction> Convert(const std::vector<tac::Instruction>);

    std::list<Instruction> m_instructions;
};

}; // assembly
