#ifndef ASM_NODES_H
#define ASM_NODES_H

#include "macro.h"
#include <string>
#include <vector>

namespace assembly {

#define ASM_OPERAND_LIST(X) \
    X(Reg, std::string name;) \
    X(Imm, int value;)

#define ASM_INSTRUCTION_LIST(X) \
    X(Mov, Operand src; Operand dst;) \
    X(Ret, /* no op */) \
    X(Function, std::string name; std::vector<Instruction> instructions;)

DEFINE_NODES_WITH_COMMON_VARIANT(Operand, ASM_OPERAND_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(Instruction, ASM_INSTRUCTION_LIST);

}; // assembly

#endif // ASM_NODES_H
