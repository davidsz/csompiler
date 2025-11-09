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
                currentOffset += 4;
                offset = -currentOffset;
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
            } else if constexpr (std::is_same_v<T, Idiv>)
                resolvePseudo(obj.src);
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
                    newAsm.push_back(Mov{obj.src, Reg{"r10d"}});
                    newAsm.push_back(Mov{Reg{"r10d"}, obj.dst});
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Binary>) {
                if (obj.op == AddAB || obj.op == SubAB) {
                    // ADD and SUB can't have memory addresses both in source and destination
                    if (std::holds_alternative<Stack>(obj.src) &&
                        std::holds_alternative<Stack>(obj.dst)) {
                        newAsm.push_back(Mov{obj.src, Reg{"r10d"}});
                        newAsm.push_back(Binary{obj.op, Reg{"r10d"}, obj.dst});
                    } else
                        newAsm.push_back(inst);
                } else if (obj.op == MultAB) {
                    // IMUL can't use memory address as its destination
                    if (std::holds_alternative<Stack>(obj.dst)) {
                        newAsm.push_back(Mov{obj.dst, Reg{"r11d"}});
                        newAsm.push_back(Binary{obj.op, obj.src, Reg{"r11d"}});
                        newAsm.push_back(Mov{Reg{"r11d"}, obj.dst});
                    } else
                        newAsm.push_back(inst);
                } else
                    newAsm.push_back(inst);
            } else if constexpr (std::is_same_v<T, Idiv>) {
                // IDIV can't have constant operand
                if (std::holds_alternative<Imm>(obj.src)) {
                    newAsm.push_back(Mov{obj.src, Reg{"r10d"}});
                    newAsm.push_back(Idiv{Reg{"r10d"}});
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
