#include "operator.h"
#include <cassert>
#include <unordered_map>

static const std::unordered_map<std::string_view, BinaryOperator> s_binary_map = {
#define ADD_TO_MAP(name, str, prec, asm) {str, BinaryOperator::name},
    BINARY_OPERATOR_LIST(ADD_TO_MAP)
#undef ADD_TO_MAP
};

static const std::unordered_map<std::string_view, UnaryOperator> s_unary_map = {
#define ADD_TO_MAP(name, str, prec, asm) {str, UnaryOperator::name},
    UNARY_OPERATOR_LIST(ADD_TO_MAP)
#undef ADD_TO_MAP
};

BinaryOperator toBinaryOperator(std::string_view s)
{
    if (auto it = s_binary_map.find(s); it != s_binary_map.end())
        return it->second;
    return BinaryOperator::UnknownBinary;
}

UnaryOperator toUnaryOperator(std::string_view s)
{
    if (auto it = s_unary_map.find(s); it != s_unary_map.end())
        return it->second;
    return UnaryOperator::UnknownUnary;
}

bool isBinaryOperator(std::string_view op)
{
    return s_binary_map.contains(op);
}

bool isUnaryOperator(std::string_view op)
{
    return s_unary_map.contains(op);
}

std::string_view toString(UnaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) case UnaryOperator::name: return str;
        UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

std::string_view toString(ASMUnaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str) case ASMUnaryOperator::name: return str;
        ASM_UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

std::string_view toString(ASMBinaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str) case ASMBinaryOperator::name: return str;
        ASM_BINARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

ASMUnaryOperator toASMUnaryOperator(UnaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) \
    case UnaryOperator::name: \
        return ASMUnaryOperator::asm;
    UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
}

ASMBinaryOperator toASMBinaryOperator(BinaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) \
    case BinaryOperator::name: \
        return ASMBinaryOperator::asm;
    BINARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
}

std::string_view toString(BinaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) case BinaryOperator::name: return str;
        BINARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

int getPrecedence(BinaryOperator op)
{
    switch (op) {
#define CASE_TO_INT(name, str, prec, asm) case BinaryOperator::name: return prec;
        BINARY_OPERATOR_LIST(CASE_TO_INT)
#undef CASE_TO_INT
    }
    assert(false);
    return -1;
}
