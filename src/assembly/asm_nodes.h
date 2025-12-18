#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include "common/types.h"
#include "common/values.h"
#include <list>
#include <string>

namespace assembly {

#define ASM_REGISTER_LIST(X) \
    X(AX, "rax", "eax", "al") \
    X(CX, "rcx", "ecx", "cl") \
    X(DX, "rdx", "edx", "dl") \
    X(DI, "rdi", "edi", "dil") \
    X(SI, "rsi", "esi", "sil") \
    X(R8, "r8", "r8d", "r8b") \
    X(R9, "r9", "r9d", "r9b") \
    X(R10, "r10", "r10d", "r10b") \
    X(R11, "r11", "r11d", "r11b") \
    X(SP, "rsp", "rsp", "rsp")

#define ASM_OPERAND_LIST(X) \
    X(Reg, \
        Register reg; uint8_t bytes = 4;) \
    X(Imm, \
        long value;) \
    X(Pseudo, \
        std::string name;) \
    X(Stack, \
        int offset;) \
    X(Data, \
        std::string name;)

#define ASM_INSTRUCTION_LIST(X) \
    X(Comment, \
        std::string text;) \
    X(Mov, \
        Operand src; \
        Operand dst; \
        WordType type;) \
    X(Movsx, \
        Operand src; \
        Operand dst;) \
    X(Ret, /* no op */) \
    X(Unary, \
        ASMUnaryOperator op; \
        Operand src; \
        WordType type;) \
    X(Binary, \
        ASMBinaryOperator op; \
        Operand src; \
        Operand dst; \
        WordType type;) \
    X(Idiv, \
        Operand src; \
        WordType type;) \
    X(Cdq, \
        WordType type;) \
    X(Cmp, \
        Operand lhs; \
        Operand rhs; \
        WordType type;) \
    X(Jmp, \
        std::string identifier;) \
    X(JmpCC, \
        std::string cond_code; \
        std::string identifier;) \
    X(SetCC, \
        std::string cond_code; \
        Operand op;) \
    X(Label, \
        std::string identifier;) \
    X(Push, \
        Operand op;) \
    X(Call, \
        std::string identifier;)

#define ASM_TOP_LEVEL_LIST(X) \
    X(Function, \
        std::string name; \
        bool global; \
        int stackSize; \
        std::list<Instruction> instructions;) \
    X(StaticVariable, \
        std::string name; \
        bool global; \
        ConstantValue init; \
        int alignment;)

enum WordType {
    Longword,
    Quadword
};

enum Register {
#define ADD_REG_TO_ENUM(name, eightbytename, fourbytename, onebytename) name,
    ASM_REGISTER_LIST(ADD_REG_TO_ENUM)
#undef ADD_REG_TO_ENUM
};

DEFINE_NODES_WITH_COMMON_VARIANT(Operand, ASM_OPERAND_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(Instruction, ASM_INSTRUCTION_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(TopLevel, ASM_TOP_LEVEL_LIST);

}; // assembly
