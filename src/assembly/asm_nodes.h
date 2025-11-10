#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include <string>
#include <vector>

namespace assembly {

#define ASM_OPERAND_LIST(X) \
    X(Reg, std::string name;) \
    X(Imm, int value;) \
    X(Pseudo, std::string name;) \
    X(Stack, int offset;)

#define ASM_INSTRUCTION_LIST(X) \
    X(Mov, Operand src; Operand dst;) \
    X(Ret, /* no op */) \
    X(Unary, ASMUnaryOperator op; Operand src;) \
    X(Binary, ASMBinaryOperator op; Operand src; Operand dst;) \
    X(Idiv, Operand src;) \
    X(Cdq, /* no op */) \
    X(Function, std::string name; int stackSize; std::vector<Instruction> instructions;)

DEFINE_NODES_WITH_COMMON_VARIANT(Operand, ASM_OPERAND_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(Instruction, ASM_INSTRUCTION_LIST);

}; // assembly
