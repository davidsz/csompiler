#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include "common/types.h"
#include "common/values.h"
#include <cassert>
#include <string>
#include <vector>

namespace tac {

#define TAC_VALUE_TYPE_LIST(X) \
    X(Constant, \
        ConstantValue value;) \
    X(Variant, \
        std::string name;)

#define TAC_INSTRUCTION_LIST(X) \
    X(Return, \
        Value val;) \
    X(Unary, \
        UnaryOperator op; \
        Value src; \
        Value dst;) \
    X(Binary, \
        BinaryOperator op; \
        Value src1; \
        Value src2; \
        Value dst;) \
    X(Copy, \
        Value src; \
        Value dst;) \
    X(GetAddress, \
        Value src; \
        Value dst;) \
    X(Load, \
        Value src_ptr; \
        Value dst;) \
    X(Store, \
        Value src; \
        Value dst_ptr;) \
    X(Jump, \
        std::string target;) \
    X(JumpIfZero, \
        Value condition; \
        std::string target;) \
    X(JumpIfNotZero, \
        Value condition; \
        std::string target;) \
    X(Label, \
        std::string identifier;) \
    X(FunctionCall, \
        std::string identifier; \
        std::vector<Value> args; \
        Value dst;) \
    X(SignExtend, \
        Value src; \
        Value dst;) \
    X(Truncate, \
        Value src; \
        Value dst;) \
    X(ZeroExtend, \
        Value src; \
        Value dst;) \
    X(DoubleToInt, \
        Value src; \
        Value dst;) \
    X(DoubleToUInt, \
        Value src; \
        Value dst;) \
    X(IntToDouble, \
        Value src; \
        Value dst;) \
    X(UIntToDouble, \
        Value src; \
        Value dst;) \
    X(AddPtr, \
        Value ptr; \
        Value index; \
        int scale; \
        Value dst;) \
    X(CopyToOffset, \
        Value src; \
        std::string dst_identifier; \
        int offset;)

#define TAC_TOP_LEVEL_LIST(X) \
    X(FunctionDefinition, \
        std::string name; \
        bool global; \
        std::vector<std::string> params; \
        std::vector<Instruction> inst;) \
    X(StaticVariable, \
        std::string name; \
        Type type = Type{}; \
        bool global; \
        std::vector<ConstantValue> list;)

DEFINE_NODES_WITH_COMMON_VARIANT(Value, TAC_VALUE_TYPE_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(Instruction, TAC_INSTRUCTION_LIST);
DEFINE_NODES_WITH_COMMON_VARIANT(TopLevel, TAC_TOP_LEVEL_LIST);


}; // tac
