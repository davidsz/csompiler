#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include <map>

namespace assembly {

// Replace each pseudo-register with proper stack offsets;
// calculates the overall stack size needed to store all local variables.
static int postprocessStackVariables(std::vector<Instruction> &asmVector)
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

    for (auto &inst : asmVector) {
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

static void postprocessInvalidInstructions(std::vector<Instruction> &asmVector)
{
    std::vector<Instruction> newAsm;
    newAsm.reserve(asmVector.size() + 10);

    for (auto &inst : asmVector) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Mov>) {
                // MOV instruction can't have memory addresses both in source and destination
                if (std::holds_alternative<Stack>(obj.src) &&
                    std::holds_alternative<Stack>(obj.dst)) {
                    newAsm.push_back(Mov{obj.src, Reg{Register::R10}});
                    newAsm.push_back(Mov{Reg{Register::R10}, obj.dst});
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Cmp>) {
                if (std::holds_alternative<Stack>(obj.lhs) &&
                    std::holds_alternative<Stack>(obj.rhs)) {
                    // CMP instruction can't have memory addresses both in source and destination
                    newAsm.push_back(Mov{obj.lhs, Reg{Register::R10}});
                    newAsm.push_back(Cmp{Reg{Register::R10}, obj.rhs});
                } else if (std::holds_alternative<Imm>(obj.rhs)) {
                    // The second operand of CMP can't be a constant
                    newAsm.push_back(Mov{obj.rhs, Reg{Register::R11}});
                    newAsm.push_back(Cmp{obj.lhs, Reg{Register::R11}});
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, SetCC>) {
                // SetCC always uses the 1-byte version of the registers
                if (auto r = std::get_if<Reg>(&obj.op))
                    newAsm.push_back(SetCC{obj.cond_code, Reg{r->reg, 1}});
                else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Binary>) {
                if (obj.op == Add_AB
                    || obj.op == Sub_AB
                    || obj.op == BWAnd_AB
                    || obj.op == BWXor_AB
                    || obj.op == BWOr_AB) {
                    // These instructions can't have memory addresses both in source and destination
                    // TODO: Second operand can't be constant?
                    if (std::holds_alternative<Stack>(obj.src) &&
                        std::holds_alternative<Stack>(obj.dst)) {
                        newAsm.push_back(Mov{obj.src, Reg{Register::R10}});
                        newAsm.push_back(Binary{obj.op, Reg{Register::R10}, obj.dst});
                    } else
                        newAsm.push_back(inst);
                } else if (obj.op == Mult_AB) {
                    // IMUL can't use memory address as its destination
                    // TODO: Second operand can't be constant?
                    if (std::holds_alternative<Stack>(obj.dst)) {
                        newAsm.push_back(Mov{obj.dst, Reg{Register::R11}});
                        newAsm.push_back(Binary{obj.op, obj.src, Reg{Register::R11}});
                        newAsm.push_back(Mov{Reg{Register::R11}, obj.dst});
                    } else
                        newAsm.push_back(inst);
                } else if (obj.op == ShiftL_AB || obj.op == ShiftRU_AB || obj.op == ShiftRS_AB) {
                    // SHL, SHR and SAR can only have constant or CL register on their left (count)
                    if (std::holds_alternative<Stack>(obj.src)) {
                        newAsm.push_back(Mov{obj.src, Reg{Register::CX}});
                        newAsm.push_back(Binary{obj.op, Reg{Register::CX, 1}, obj.dst});
                    } else
                        newAsm.push_back(inst);
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Idiv>) {
                // IDIV can't have constant operand
                if (std::holds_alternative<Imm>(obj.src)) {
                    newAsm.push_back(Mov{obj.src, Reg{Register::R10}});
                    newAsm.push_back(Idiv{Reg{Register::R10}});
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Function>) {
                auto newFunc = obj;
                postprocessInvalidInstructions(newFunc.instructions);
                newAsm.push_back(newFunc);
            } else {
                newAsm.push_back(inst);
            }
        }, inst);
    }
    asmVector = std::move(newAsm);
}

std::string from_tac(std::vector<tac::Instruction> tacVector)
{
    ASMBuilder tacToAsm;
    std::vector<Instruction> asmVector = tacToAsm.Convert(tacVector);

    postprocessStackVariables(asmVector);
    postprocessInvalidInstructions(asmVector);

    ASMPrinter asmPrinter;
    return asmPrinter.ToText(asmVector);
}

}; // assembly
