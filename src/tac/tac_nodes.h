#ifndef TAC_NODES_H
#define TAC_NODES_H

#include "macro.h"
#include <cassert>
#include <string>
#include <variant>
#include <vector>

namespace tac {

struct Empty {};

enum UnaryOperator {
    Negate,
    Increment,
    Decrement,
};

#define TAC_INSTRUCTION_LIST(X) \
    X(Return, Value val;) \
    X(Unary, UnaryOperator op; Value src; Value dst;)

#define TAC_VALUE_TYPE_LIST(X) \
    X(Constant, int value;) \
    X(Variant, std::string name;)

#define TAC_OTHER_TYPE_LIST(X) \
    X(FunctionDefinition, std::string name; std::vector<Instruction> inst;)


TAC_VALUE_TYPE_LIST(DEFINE_NODE)
using Value = std::variant<
    TAC_VALUE_TYPE_LIST(ADD_TO_VARIANT)
    Empty
>;

TAC_INSTRUCTION_LIST(DEFINE_NODE)
using Instruction = std::variant<
    TAC_INSTRUCTION_LIST(ADD_TO_VARIANT)
    Empty
>;

TAC_OTHER_TYPE_LIST(DEFINE_NODE)

using Any = std::variant<
    TAC_VALUE_TYPE_LIST(ADD_TO_VARIANT)
    TAC_INSTRUCTION_LIST(ADD_TO_VARIANT)
    TAC_OTHER_TYPE_LIST(ADD_TO_VARIANT)
    Empty
>;

template <typename T>
T unwrap(Any &&value)
{
    if (auto ptr = std::get_if<T>(&value))
        return std::move(*ptr);
    assert(false);
    return T{};
}

}; // tac

#endif // TAC_NODES_H
