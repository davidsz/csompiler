#pragma once

#include <string_view>

#define UNARY_OPERATOR_LIST(X) \
    X(Negate, "-")             \
    X(Decrement, "--")         \
    X(BitwiseComplement, "~")

enum UnaryOperator {
#define DEFINE_ENUM(name, str) name,
    UNARY_OPERATOR_LIST(DEFINE_ENUM)
#undef DEFINE_ENUM
};

UnaryOperator toUnaryOperator(std::string_view s);
std::string_view toString(UnaryOperator op);
