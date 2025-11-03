#pragma once

#include "macro.h"
#include <string_view>

#define UNARY_OPERATOR_LIST(X) \
    X(Negate, "-") \
    X(Decrement, "--") \
    X(BitwiseComplement, "~")

#define ASM_UNARY_OPERATOR_LIST(X) \
    X(Neg, "negl") \
    X(Not, "notl") \
    X(Unknown, "UNKNOWN_OP")

#define UNARY_CONVERSION_LIST(X) \
    X(Negate, Neg) \
    X(Decrement, Unknown) \
    X(BitwiseComplement, Not)

DEFINE_ENUM(UnaryOperator, UNARY_OPERATOR_LIST);
DEFINE_ENUM(ASMUnaryOperator, ASM_UNARY_OPERATOR_LIST);

UnaryOperator toUnaryOperator(std::string_view s);
std::string_view toString(UnaryOperator op);
std::string_view toString(ASMUnaryOperator op);
ASMUnaryOperator toASMUnaryOperator(UnaryOperator op);
