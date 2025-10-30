#ifndef TAC_NODES_H
#define TAC_NODES_H

#include <string>
#include <variant>
#include <vector>

namespace tac {

struct Empty {};

#define STATEMENT_LIST(X) \
    X(FunctionDef, std::string name; std::vector<Instruction> instructions;)

#define INSTRUCTION_LIST(X) \
    X(Return, Val value;) \
    X(Unary, UnaryOperator op; Val src; Val dst;)

#define VALUE_TYPE_LIST(X) \
    X(Constant, int value;) \
    X(Identifier, std::string name;)

enum UnaryOperator {
    Complement,
    Negate,
};

#define FORWARD_DECL_NODE(name, members) \
    struct name;

#define DEFINE_NODE(name, members) \
    struct name { members };

#define ADD_TO_VARIANT(name, members) name,

VALUE_TYPE_LIST(DEFINE_NODE)
using Val = std::variant<
    VALUE_TYPE_LIST(ADD_TO_VARIANT)
    Empty
>;

INSTRUCTION_LIST(DEFINE_NODE)
using Instruction = std::variant<
    INSTRUCTION_LIST(ADD_TO_VARIANT)
    Empty
>;

STATEMENT_LIST(DEFINE_NODE)

using Any = std::variant<
    STATEMENT_LIST(ADD_TO_VARIANT)
    INSTRUCTION_LIST(ADD_TO_VARIANT)
    VALUE_TYPE_LIST(ADD_TO_VARIANT)
    Empty
>;

}; // tac

#endif // TAC_NODES_H
