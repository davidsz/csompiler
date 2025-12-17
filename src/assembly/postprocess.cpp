#include "postprocess.h"
#include <map>

namespace assembly {

// Replace each pseudo-register with proper stack offsets or static variables;
// calculates the overall stack size needed to store all local variables.
static int postprocessPseudoRegisters(
    std::list<Instruction> &asmList,
    std::shared_ptr<ASMSymbolTable> asmSymbolTable)
{
    std::map<std::string, int> pseudoOffset;
    int currentOffset = 0;

    auto resolvePseudo = [&](Operand &op) {
        if (auto pseudo = std::get_if<Pseudo>(&op)) {
            ObjEntry *entry = asmSymbolTable->getAs<ObjEntry>(pseudo->name);
            if (!pseudoOffset.contains(pseudo->name)) {
                if (entry && entry->is_static) {
                    // Replace static variables with Data operands
                    op.emplace<Data>(pseudo->name);
                    return;
                }
            }
            // All other variable types are stack offsets
            int &offset = pseudoOffset[pseudo->name];
            if (offset == 0) {
                // The given pseudoregister has no offset yet
                if (entry && entry->type == WordType::Quadword) {
                    currentOffset -= 8;
                    // Quadwords should be 8-byte aligned
                    if (currentOffset % 8 != 0)
                        currentOffset -= 4;
                } else
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
            } else if constexpr (std::is_same_v<T, Movsx>) {
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
            else if constexpr (std::is_same_v<T, Push>)
                resolvePseudo(obj.op);
        }, inst);
    }

    return -currentOffset;
}

void postprocessPseudoRegisters(
    std::list<TopLevel> &asmList,
    std::shared_ptr<ASMSymbolTable> asmSymbolTable)
{
    for (auto &inst : asmList) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                obj.stackSize = postprocessPseudoRegisters(obj.instructions, asmSymbolTable);
                // Round up to the next number which is divisible by 16
                obj.stackSize = (obj.stackSize + 15) & ~15;
            }
        }, inst);
    }
}

// ------------------------

static bool isMemoryAddress(const Operand &op)
{
    return std::holds_alternative<Stack>(op)
        || std::holds_alternative<Data>(op);
}

// "The assembler permits an immediate value in addq, imulq, subq, cmpq, or pushq only if
// it can be represented as a signed 32-bit integer. That’s because these instructions all
// sign extend their immediate operands from 32 to 64 bits. If an immediate value can
// be represented in 32 bits only as an unsigned integer—which implies that its upper
// bit is set—sign extending it will change its value."
static bool isEightBytesImm(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op)) {
        return imm->value <= std::numeric_limits<int32_t>::lowest() ||
               imm->value >= std::numeric_limits<int32_t>::max();
    }
    return false;
}

static uint8_t getBytesOfWordType(WordType type)
{
    switch (type) {
    case WordType::Longword:
        return 4;
    case WordType::Quadword:
        return 8;
    default:
        return 4;
    }
}

static std::list<Instruction>::iterator postprocessMov(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Mov>(*it);
    // MOV instruction can't have memory addresses both in source and destination
    if ((isMemoryAddress(obj.src) && isMemoryAddress(obj.dst))
        || obj.type == WordType::Quadword) {
        auto current = obj;
        uint8_t bytes = getBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Mov{Reg{R10, bytes}, current.dst, current.type});
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessMovsx(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Movsx>(*it);

    // MOVSLQ's source register always 4 bytes, the destination is 8 bytes
    if (auto r = std::get_if<Reg>(&obj.src))
        *it = Movsx{Reg{r->reg, 4}, obj.dst};
    if (auto r = std::get_if<Reg>(&obj.dst))
        *it = Movsx{obj.src, Reg{r->reg, 8}};

    // MOVSX instruction can't use memory address as destination
    // or an immediate value as a source
    // TODO: Check both operands individually?
    if (std::holds_alternative<Imm>(obj.src) || isMemoryAddress(obj.dst)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{R10, 8}, WordType::Quadword});
        it = asmList.emplace(std::next(it), Movsx{Reg{R10, 4}, Reg{R11, 8}});
        it = asmList.emplace(std::next(it), Mov{Reg{R11, 8}, current.dst, WordType::Quadword});
    }

    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCmp(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cmp>(*it);
    if (std::holds_alternative<Imm>(obj.rhs)
        && obj.type == WordType::Quadword) {
        // TODO: Implement this complex case in a nicer way
        // Both rule applies at once
        auto current = obj;
        uint8_t bytes = getBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Mov{current.rhs, Reg{R11, bytes}, current.type});
        it = asmList.emplace(std::next(it), Cmp{Reg{R10, bytes}, Reg{R11, bytes}, current.type});
    } else if ((isMemoryAddress(obj.lhs) && isMemoryAddress(obj.rhs))
        || obj.type == WordType::Quadword) {
        // CMP instruction can't have memory addresses both in source and destination
        auto current = obj;
        uint8_t bytes = getBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Cmp{Reg{R10, bytes}, current.rhs, current.type});
    } else if (std::holds_alternative<Imm>(obj.rhs)) {
        // The second operand of CMP can't be a constant
        auto current = obj;
        uint8_t bytes = getBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.rhs, Reg{R11, bytes}, current.type});
        it = asmList.emplace(std::next(it), Cmp{current.lhs, Reg{R11, bytes}, current.type});
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

static std::list<Instruction>::iterator postprocessPush(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Push>(*it);
    // PUSHQ can handle quadwords only
    if (isEightBytesImm(obj.op)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.op, Reg{R10, 8}, WordType::Quadword});
        it = asmList.emplace(std::next(it), Push{Reg{R10, 8}});
    }
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
        // ADDQ and SUBQ can't handle immediate values that can't fit into an int
        // TODO: Second operand can't be constant?
        if ((isMemoryAddress(obj.src) && isMemoryAddress(obj.dst))
            || obj.type == WordType::Quadword) {
            auto current = obj;
            uint8_t bytes = getBytesOfWordType(current.type);
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{R10, bytes}, current.dst, current.type});
        }
    } else if (obj.op == Mult_AB) {
        // IMUL can't use memory address as its destination
        // IMULQ can't handle immediate values that can't fit into an int
        // TODO: Second operand can't be constant?
        // TODO: Make it nicer
        if (obj.type == WordType::Quadword && isMemoryAddress(obj.dst)) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{R10, 8}, current.type});
            it = asmList.emplace(std::next(it), Mov{current.dst, Reg{R11, 8}, current.type});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{R10, 8}, Reg{R11, 8}, WordType::Quadword});
            it = asmList.emplace(std::next(it), Mov{Reg{R11, 8}, current.dst, current.type});
        } else if (isMemoryAddress(obj.dst)) {
            auto current = obj;
            uint8_t bytes = getBytesOfWordType(current.type);
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.dst, Reg{R11, bytes}, current.type});
            it = asmList.emplace(std::next(it), Binary{current.op, current.src, Reg{R11, bytes}, current.type});
            it = asmList.emplace(std::next(it), Mov{Reg{R11, bytes}, current.dst, current.type});
        } else if (obj.type == WordType::Quadword) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{R10, 8}, WordType::Quadword});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{R10, 8}, current.dst, WordType::Quadword});
        }
    } else if (obj.op == ShiftL_AB || obj.op == ShiftRU_AB || obj.op == ShiftRS_AB) {
        // SHL, SHR and SAR can only have constant or CL register on their left (count)
        if (isMemoryAddress(obj.src)) {
            auto current = obj;
            uint8_t bytes = getBytesOfWordType(current.type);
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{current.src, Reg{CX, bytes}, current.type});
            it = asmList.emplace(std::next(it), Binary{current.op, Reg{CX, 1}, current.dst, current.type});
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
        uint8_t bytes = getBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Idiv{Reg{R10, bytes}, current.type});
    }
    return std::next(it);
}

static void postprocessInvalidInstructions(std::list<Instruction> &asmList)
{
    for (auto it = asmList.begin(); it != asmList.end();) {
        it = std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Mov>)
                return postprocessMov(asmList, it);
            else if constexpr (std::is_same_v<T, Movsx>)
                return postprocessMovsx(asmList, it);
            else if constexpr (std::is_same_v<T, Cmp>)
                return postprocessCmp(asmList, it);
            else if constexpr (std::is_same_v<T, SetCC>)
                return postprocessSetCC(asmList, it);
            else if constexpr (std::is_same_v<T, Push>)
                return postprocessPush(asmList, it);
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

void postprocessInvalidInstructions(std::list<TopLevel> &asmList)
{
    for (auto it = asmList.begin(); it != asmList.end();) {
        it = std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                postprocessInvalidInstructions(obj.instructions);
                return std::next(it);
            } else
                return std::next(it);
        }, *it);
    }
}

}; // namespace assembly
