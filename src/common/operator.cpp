#include "operator.h"
#include "common/system.h"
#include <cassert>
#include <format>
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

std::string toString(ASMUnaryOperator op, WordType type)
{
    switch (op) {
#define CASE_TO_STRING(enum_name, text_name) case ASMUnaryOperator::enum_name: return AddSuffix(text_name, type);
    ASM_UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

std::string toString(ASMBinaryOperator op, WordType type)
{
    if (type == Doubleword) {
        if (op == Mult_AB) return "mulsd";
        if (op == BWXor_AB) return "xorpd";
    }

    switch (op) {
#define CASE_TO_STRING(enum_name, text_name) case ASMBinaryOperator::enum_name: return AddSuffix(text_name, type);
    ASM_BINARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

std::string AddSuffix(std::string_view instruction, WordType type)
{
    switch (type) {
    case Longword:   return std::format("{}l", instruction);
    case Quadword:   return std::format("{}q", instruction);
    case Doubleword: return std::format("{}sd", instruction);
    default: assert(false); return "";
    }
}

ASMUnaryOperator toASMUnaryOperator(UnaryOperator op)
{
    ASMUnaryOperator ret;
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) \
    case UnaryOperator::name: ret = ASMUnaryOperator::asm; break;
    UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    return ret;
}

ASMBinaryOperator toASMBinaryOperator(BinaryOperator op, WordType wordType, bool isSigned)
{
    ASMBinaryOperator ret;
    switch (op) {
#define CASE_TO_STRING(name, str, prec, asm) \
    case BinaryOperator::name: \
        ret = ASMBinaryOperator::asm; break;
    BINARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }

    if (op == BinaryOperator::Divide && wordType == Doubleword)
        return DivDouble_AB;

    if (ret == ASMBinaryOperator::ShiftRS_AB && !isSigned)
        ret = ASMBinaryOperator::ShiftRU_AB;

    return ret;
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

int getPrecedence(UnaryOperator op)
{
    switch (op) {
#define CASE_TO_INT(name, str, prec, asm) case UnaryOperator::name: return prec;
        UNARY_OPERATOR_LIST(CASE_TO_INT)
#undef CASE_TO_INT
    }
    assert(false);
    return -1;
}

DIAG_PUSH
DIAG_IGNORE("-Wswitch-enum")
bool isAssignment(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::LeftShift:
        case BinaryOperator::RightShift:
        case BinaryOperator::Assign:
        case BinaryOperator::AssignAdd:
        case BinaryOperator::AssignSub:
        case BinaryOperator::AssignMult:
        case BinaryOperator::AssignDiv:
        case BinaryOperator::AssignMod:
        case BinaryOperator::AssignLShift:
        case BinaryOperator::AssignRShift:
        case BinaryOperator::AssignBitwiseAnd:
        case BinaryOperator::AssignBitwiseXor:
        case BinaryOperator::AssignBitwiseOr:
            return true;
        default:
            return false;
    }
}

bool isCompoundAssignment(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::AssignAdd:
        case BinaryOperator::AssignSub:
        case BinaryOperator::AssignMult:
        case BinaryOperator::AssignDiv:
        case BinaryOperator::AssignMod:
        case BinaryOperator::AssignLShift:
        case BinaryOperator::AssignRShift:
        case BinaryOperator::AssignBitwiseAnd:
        case BinaryOperator::AssignBitwiseXor:
        case BinaryOperator::AssignBitwiseOr:
            return true;
        default:
            return false;
    }
}

bool isRelationOperator(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::Equal:
        case BinaryOperator::NotEqual:
        case BinaryOperator::LessThan:
        case BinaryOperator::LessOrEqual:
        case BinaryOperator::GreaterThan:
        case BinaryOperator::GreaterOrEqual:
            return true;
        default:
            return false;
    }
}

BinaryOperator compoundToBinary(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::AssignAdd: return BinaryOperator::Add;
        case BinaryOperator::AssignSub: return BinaryOperator::Subtract;
        case BinaryOperator::AssignMult: return BinaryOperator::Multiply;
        case BinaryOperator::AssignDiv: return BinaryOperator::Divide;
        case BinaryOperator::AssignMod: return BinaryOperator::Remainder;
        case BinaryOperator::AssignLShift: return BinaryOperator::LeftShift;
        case BinaryOperator::AssignRShift: return BinaryOperator::RightShift;
        case BinaryOperator::AssignBitwiseAnd: return BinaryOperator::BitwiseAnd;
        case BinaryOperator::AssignBitwiseXor: return BinaryOperator::BitwiseXor;
        case BinaryOperator::AssignBitwiseOr: return BinaryOperator::BitwiseOr;
        default:
            return BinaryOperator::UnknownBinary;
    }
}

BinaryOperator unaryToBinary(UnaryOperator op)
{
    switch (op) {
        case UnaryOperator::Increment: return BinaryOperator::Add;
        case UnaryOperator::Decrement: return BinaryOperator::Subtract;
        default:
            return BinaryOperator::UnknownBinary;
    }
}
DIAG_POP

bool canBePostfix(UnaryOperator op)
{
    return op == UnaryOperator::Increment || op == UnaryOperator::Decrement;
}
