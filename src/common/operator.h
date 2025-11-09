#pragma once

#include "macro.h"
#include <string_view>

#define BINARY_OPERATOR_LIST(X) \
    X(UnknownBinary, "", 0) \
    X(Multiply, "*", 10) \
    X(Divide, "/", 10) \
    X(Remainder, "%", 10) \
    X(Add, "+", 9) \
    X(Substract, "-", 9) \
    X(LeftShift, "<<", 8) \
    X(RightShift, ">>", 8) \
    X(BitwiseAnd, "&", 7) \
    X(BitwiseXor, "^", 6) \
    X(BitwiseOr, "|", 5)

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

/*
FIXME: Use the proper right shift command in assembly
once we have information about the signedness of the
operand expressions
	- Signed left operand: sarl
	- Unsigned left operand: shrl
*/
#define ASM_BINARY_OPERATOR_LIST(X) \
    X(UnknownAB, "UNKNOWN_OP", 0) \
    X(AddAB, "addl", 0) \
    X(SubAB, "subl", 0) \
    X(MultAB, "imull", 0) \
    X(ShiftLAB, "shll", 0) \
    X(ShiftRUAB, "shrl", 0) \
    X(ShiftRSAB, "sarl", 0) \
    X(BWAndAB, "andl", 0) \
    X(BWXorAB, "xorl", 0) \
    X(BWOrAB, "orl", 0)

#define BINARY_CONVERSION_LIST(X) \
    X(UnknownBinary, UnknownAB) \
    X(Multiply, MultAB) \
    X(Divide, UnknownAB) \
    X(Remainder, UnknownAB) \
    X(Add, AddAB) \
    X(Substract, SubAB) \
    X(LeftShift, ShiftLAB) \
    X(RightShift, ShiftRUAB) \
    X(BitwiseAnd, BWAndAB) \
    X(BitwiseXor, BWXorAB) \
    X(BitwiseOr, BWOrAB)

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
