#pragma once

#include "macro.h"
#include <string_view>

#define BINARY_OPERATOR_LIST(X) \
    X(UnknownBinary, "", 0, Unknown_AB) \
    X(Multiply, "*", 70, Mult_AB) \
    X(Divide, "/", 70, Unknown_AB) \
    X(Remainder, "%", 70, Unknown_AB) \
    X(Add, "+", 60, Add_AB) \
    X(Subtract, "-", 60, Sub_AB) \
    X(LeftShift, "<<", 50, ShiftL_AB) \
    X(RightShift, ">>", 50, ShiftRU_AB) \
    X(LessThan, "<", 40, Unknown_AB) \
    X(LessOrEqual, "<=", 40, Unknown_AB) \
    X(GreaterThan, ">", 40, Unknown_AB) \
    X(GreaterOrEqual, ">=", 40, Unknown_AB) \
    X(Equal, "==", 35, Unknown_AB) \
    X(NotEqual, "!=", 35, Unknown_AB) \
    X(BitwiseAnd, "&", 30, BWAnd_AB) \
    X(BitwiseXor, "^", 25, BWXor_AB) \
    X(BitwiseOr, "|", 20, BWOr_AB) \
    X(And, "&&", 15, Unknown_AB) \
    X(Or, "||", 10, Unknown_AB) \
    X(Assign, "=", 1, Unknown_AB)

#define UNARY_OPERATOR_LIST(X) \
    X(UnknownUnary, "", 0, Unknown_AU) \
    X(Negate, "-", 0, Neg_AU) \
    X(Decrement, "--", 0, Unknown_AU) \
    X(BitwiseComplement, "~", 0, Not_AU) \
    X(Not, "!", 0, Unknown_AU)

#define ASM_UNARY_OPERATOR_LIST(X) \
    X(Unknown_AU, "UNKNOWN_OP") \
    X(Neg_AU, "negl") \
    X(Not_AU, "notl")

/*
FIXME: Use the proper right shift command in assembly
once we have information about the signedness of the
operand expressions
	- Signed left operand: sarl
	- Unsigned left operand: shrl
*/
#define ASM_BINARY_OPERATOR_LIST(X) \
    X(Unknown_AB, "UNKNOWN_OP") \
    X(Add_AB, "addl") \
    X(Sub_AB, "subl") \
    X(Mult_AB, "imull") \
    X(ShiftL_AB, "shll") \
    X(ShiftRU_AB, "shrl") \
    X(ShiftRS_AB, "sarl") \
    X(BWAnd_AB, "andl") \
    X(BWXor_AB, "xorl") \
    X(BWOr_AB, "orl")

DEFINE_OPERATOR(BinaryOperator, BINARY_OPERATOR_LIST);
DEFINE_OPERATOR(UnaryOperator, UNARY_OPERATOR_LIST);
DEFINE_ASM_OPERATOR(ASMUnaryOperator, ASM_UNARY_OPERATOR_LIST);
DEFINE_ASM_OPERATOR(ASMBinaryOperator, ASM_BINARY_OPERATOR_LIST);

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
