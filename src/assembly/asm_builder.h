#pragma once

#include "asm_nodes.h"
#include "tac/tac_visitor.h"

namespace assembly {

struct ASMBuilder : public tac::ITACVisitor<Operand> {
    Operand operator()(const tac::FunctionDefinition &) override;
    Operand operator()(const tac::Return &) override;
    Operand operator()(const tac::Unary &) override;
    Operand operator()(const tac::Constant &) override;
    Operand operator()(const tac::Variant &) override;
    Operand operator()(std::monostate) override { assert(false); }

    std::vector<Instruction> Convert(const std::vector<tac::Instruction>);

    std::vector<Instruction> m_instructions;
};

}; // assembly
