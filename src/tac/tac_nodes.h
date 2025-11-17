#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include <cassert>
#include <string>
#include <vector>

namespace tac {

#define TAC_INSTRUCTION_LIST(X) \
    X(Return, Value val;) \
    X(Unary, UnaryOperator op; Value src; Value dst;) \
    X(Binary, BinaryOperator op; Value src1; Value src2; Value dst;) \
    X(FunctionDefinition, std::string name; std::vector<Instruction> inst;) \
    X(Copy, Value src; Value dst;) \
    X(Jump, std::string target;) \
    X(JumpIfZero, Value condition; std::string target;) \
    X(JumpIfNotZero, Value condition; std::string target;) \
    X(Label, std::string identifier;)

#define TAC_VALUE_TYPE_LIST(X) \
    X(Constant, int value;) \
    X(Variant, std::string name;)

DEFINE_NODES_WITH_COMMON_VARIANT(Value, TAC_VALUE_TYPE_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(Instruction, TAC_INSTRUCTION_LIST);

}; // tac
