#ifndef ASM_NODES_H
#define ASM_NODES_H

#include <cassert>
#include <string>
#include <variant>
#include <vector>

namespace assembly {

using Register = std::string;
using Imm = int; // Immediate value
using Operand = std::variant<Imm, Register>;

struct Mov {
    Operand src;
    Operand dst;
};

struct Ret {};

using Instruction = std::variant<
    Mov,
    Ret
>;
using InstructionList = std::vector<Instruction>;

struct FuncDecl {
    std::string name;
    InstructionList instructions;
};

using Any = std::variant<Register, Imm, Mov, Ret, FuncDecl, InstructionList>;

template <typename T>
T unwrap(Any &&value)
{
    if (auto ptr = std::get_if<T>(&value))
        return std::move(*ptr);
    assert(false);
    return T{};
}

}; // assembly

#endif // ASM_NODES_H
