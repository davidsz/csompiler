#pragma once

#include "macro.h"
#include <string_view>

#define BINARY_OPERATOR_LIST(X) \
    X(UnknownBinary, "", 0) \
    X(Substract, "-", 4) \
    X(Add, "+", 4) \
    X(Multiply, "*", 5) \
    X(Divide, "/", 5) \
    X(Remainder, "%", 5)

#define UNARY_OPERATOR_LIST(X) \
    X(UnknownUnary, "", 0) \
    X(Negate, "-", 0) \
    X(Decrement, "--", 0) \
    X(BitwiseComplement, "~", 0)

#define ASM_UNARY_OPERATOR_LIST(X) \
    X(UnknownAU, "UNKNOWN_OP", 0) \
    X(Neg, "negl", 0) \
    X(Not, "notl", 0)

#define UNARY_CONVERSION_LIST(X) \
    X(UnknownUnary, UnknownAU) \
    X(Negate, Neg) \
    X(Decrement, UnknownAU) \
    X(BitwiseComplement, Not)

#define ASM_BINARY_OPERATOR_LIST(X) \
    X(UnknownAB, "UNKNOWN_OP", 0) \
    X(AddAB, "addl", 0) \
    X(SubAB, "subl", 0) \
    X(MultAB, "imull", 0)

#define BINARY_CONVERSION_LIST(X) \
    X(UnknownBinary, UnknownAB) \
    X(Substract, SubAB) \
    X(Add, AddAB) \
    X(Multiply, MultAB) \
    X(Divide, UnknownAB) \
    X(Remainder, UnknownAB)

DEFINE_ENUM(BinaryOperator, BINARY_OPERATOR_LIST);
DEFINE_ENUM(UnaryOperator, UNARY_OPERATOR_LIST);
DEFINE_ENUM(ASMUnaryOperator, ASM_UNARY_OPERATOR_LIST);
DEFINE_ENUM(ASMBinaryOperator, ASM_BINARY_OPERATOR_LIST);

std::string_view toString(BinaryOperator op);
std::string_view toString(UnaryOperator op);
std::string_view toString(ASMUnaryOperator op);
std::string_view toString(ASMBinaryOperator op);

bool isBinaryOperator(std::string_view op);
bool isUnaryOperator(std::string_view op);

BinaryOperator toBinaryOperator(std::string_view s);
UnaryOperator toUnaryOperator(std::string_view s);
ASMUnaryOperator toASMUnaryOperator(UnaryOperator op);
ASMBinaryOperator toASMBinaryOperator(BinaryOperator op);

int getPrecedence(BinaryOperator op);
