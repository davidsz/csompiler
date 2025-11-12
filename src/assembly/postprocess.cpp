#include "postprocess.h"
#include <map>

namespace assembly {

// Replace each pseudo-register with proper stack offsets;
// calculates the overall stack size needed to store all local variables.
int postprocessStackVariables(std::list<Instruction> &asmList)
{
    std::map<std::string, int> pseudoOffset;
    int currentOffset = 0;

    auto resolvePseudo = [&](Operand &op) {
        if (auto ptr = std::get_if<Pseudo>(&op)) {
            int &offset = pseudoOffset[ptr->name];
            if (offset == 0) {
                currentOffset -= 4;
                offset = currentOffset;
            }
            op.emplace<Stack>(offset);
        }
    };

    for (auto &inst : asmList) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Mov>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Unary>)
                resolvePseudo(obj.src);
            else if constexpr (std::is_same_v<T, Binary>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Idiv>) {
                resolvePseudo(obj.src);
            } else if constexpr (std::is_same_v<T, Cmp>) {
                resolvePseudo(obj.lhs);
                resolvePseudo(obj.rhs);
            } else if constexpr (std::is_same_v<T, SetCC>)
                resolvePseudo(obj.op);
            else if constexpr (std::is_same_v<T, Function>)
                obj.stackSize = postprocessStackVariables(obj.instructions);
        }, inst);
    }

    return currentOffset;
}

static std::list<Instruction>::iterator postprocessMov(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Mov>(*it);
    // MOV instruction can't have memory addresses both in source and destination
    if (std::holds_alternative<Stack>(obj.src) && std::holds_alternative<Stack>(obj.dst)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{Register::R10}});
        it = asmList.emplace(std::next(it), Mov{Reg{Register::R10}, current.dst});
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCmp(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cmp>(*it);
    if (std::holds_alternative<Stack>(obj.lhs) && std::holds_alternative<Stack>(obj.rhs)) {
        // CMP instruction can't have memory addresses both in source and destination
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.lhs, Reg{Register::R10}});
        it = asmList.emplace(std::next(it), Cmp{Reg{Register::R10}, current.rhs});
    } else if (std::holds_alternative<Imm>(obj.rhs)) {
        // The second operand of CMP can't be a constant
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.rhs, Reg{Register::R11}});
        it = asmList.emplace(std::next(it), Cmp{current.lhs, Reg{Register::R11}});
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessSetCC(std::list<Instruction> &, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<SetCC>(*it);
    // SetCC always uses the 1-byte version of the registers
    if (auto r = std::get_if<Reg>(&obj.op))
        *it = SetCC{obj.cond_code, Reg{r->reg, 1}};
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessBinary(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Binary>(*it);
    if (obj.op == Add_AB
        || obj.op == Sub_AB
        || obj.op == BWAnd_AB
        || obj.op == BWXor_AB
        || obj.op == BWOr_AB) {
        // These instructions can't have memory addresses both in source and destination
        // TODO: Second operand can't be constant?
        if (std::holds_alternative<Stack>(obj.src) &&
            std::holds_alternative<Stack>(obj.dst)) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{Register::R10}});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{Register::R10}, current.dst});
        }
    } else if (obj.op == Mult_AB) {
        // IMUL can't use memory address as its destination
        // TODO: Second operand can't be constant?
        if (std::holds_alternative<Stack>(obj.dst)) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.dst, Reg{Register::R11}});
            it = asmList.emplace(std::next(it), Binary{current.op, current.src, Reg{Register::R11}});
            it = asmList.emplace(std::next(it), Mov{Reg{Register::R11}, current.dst});
        }
    } else if (obj.op == ShiftL_AB || obj.op == ShiftRU_AB || obj.op == ShiftRS_AB) {
        // SHL, SHR and SAR can only have constant or CL register on their left (count)
        if (std::holds_alternative<Stack>(obj.src)) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{Register::CX}});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{Register::CX, 1}, current.dst});
        }
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessIdiv(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Idiv>(*it);
    // IDIV can't have constant operand
    if (std::holds_alternative<Imm>(obj.src)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{Register::R10}});
        it = asmList.emplace(std::next(it), Idiv{Reg{Register::R10}});
    }
    return std::next(it);
}

void postprocessInvalidInstructions(std::list<Instruction> &asmList)
{
    for (auto it = asmList.begin(); it != asmList.end();) {
        it = std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Mov>)
                return postprocessMov(asmList, it);
            else if constexpr (std::is_same_v<T, Cmp>)
                return postprocessCmp(asmList, it);
            else if constexpr (std::is_same_v<T, SetCC>)
                return postprocessSetCC(asmList, it);
            else if constexpr (std::is_same_v<T, Binary>)
                return postprocessBinary(asmList, it);
            else if constexpr (std::is_same_v<T, Idiv>) {
                return postprocessIdiv(asmList, it);
            } else if constexpr (std::is_same_v<T, Function>) {
                postprocessInvalidInstructions(obj.instructions);
                return std::next(it);
            } else
                return std::next(it);
        }, *it);
    }
}

}; // namespace assembly
