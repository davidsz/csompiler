#include "postprocess.h"
#include <cassert>
#include <limits>
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
        std::string name;
        int extra_offset = 0; // The offset inside the array
        if (auto pseudo = std::get_if<Pseudo>(&op))
            name = pseudo->name;
        else if (auto pseudo_aggr = std::get_if<PseudoAggregate>(&op)) {
            name = pseudo_aggr->name;
            extra_offset = pseudo_aggr->offset;
        } else
            return;

        ObjEntry *entry = asmSymbolTable->getAs<ObjEntry>(name);
        assert(entry);
        if (entry && entry->is_static) {
            // Replace static variables with Data operands
            op.emplace<Data>(name);
            return;
        }
        // All other variable types are stack offsets
        // If a variable has no stack offset yet, determine it
        if (auto it = pseudoOffset.find(name); it == pseudoOffset.end()) {
            currentOffset -= entry->type.size();
            currentOffset &= ~(entry->type.alignment() - 1);
            pseudoOffset[name] = currentOffset;
        }
        op.emplace<Memory>(BP, pseudoOffset[name] + extra_offset);
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
            } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Lea>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Unary>) {
                resolvePseudo(obj.src);
            } else if constexpr (std::is_same_v<T, Binary>) {
                resolvePseudo(obj.src);
                resolvePseudo(obj.dst);
            } else if constexpr (std::is_same_v<T, Idiv>) {
                resolvePseudo(obj.src);
            } else if constexpr (std::is_same_v<T, Div>) {
                resolvePseudo(obj.src);
            } else if constexpr (std::is_same_v<T, Cmp>) {
                resolvePseudo(obj.lhs);
                resolvePseudo(obj.rhs);
            } else if constexpr (std::is_same_v<T, SetCC>) {
                resolvePseudo(obj.op);
            } else if constexpr (std::is_same_v<T, Push>) {
                resolvePseudo(obj.op);
            }
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
    return std::holds_alternative<Memory>(op)
        || std::holds_alternative<Data>(op)
        || std::holds_alternative<Indexed>(op);
}

static bool isOneBytesImm(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op)) {
        return static_cast<int64_t>(imm->value) >= std::numeric_limits<int8_t>::lowest() ||
               static_cast<int64_t>(imm->value) <= std::numeric_limits<int8_t>::max();
    }
    return false;
}

static bool isFourBytesImm(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op)) {
        return static_cast<int64_t>(imm->value) >= std::numeric_limits<int32_t>::lowest() ||
               static_cast<int64_t>(imm->value) <= std::numeric_limits<int32_t>::max();
    }
    return false;
}

// "The assembler permits an immediate value in addq, imulq, subq, cmpq, or pushq only if
// it can be represented as a signed 32-bit integer. That’s because these instructions all
// sign extend their immediate operands from 32 to 64 bits. If an immediate value can
// be represented in 32 bits only as an unsigned integer—which implies that its upper
// bit is set—sign extending it will change its value."
static bool isEightBytesImm(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op)) {
        return static_cast<int64_t>(imm->value) <= std::numeric_limits<int32_t>::lowest() ||
               static_cast<int64_t>(imm->value) >= std::numeric_limits<int32_t>::max();
    }
    return false;
}

static bool isXMMRegister(const Operand &op)
{
    if (const Reg *r = std::get_if<Reg>(&op))
        return r->reg >= XMM0 && r->reg <= XMM15;
    return false;
}

static std::list<Instruction>::iterator postprocessMov(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Mov>(*it);
    // MOV instruction can't have memory addresses both in source and destination
    if ((isMemoryAddress(obj.src) && isMemoryAddress(obj.dst))
        || obj.type == WordType::Quadword) {
        auto current = obj;
        it = asmList.erase(it);
        if (current.type == WordType::Doubleword) {
            it = asmList.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
            it = asmList.emplace(std::next(it), Mov{Reg{XMM14, 8}, current.dst, Doubleword});
        } else {
            uint8_t bytes = GetBytesOfWordType(current.type);
            it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
            it = asmList.emplace(std::next(it), Mov{Reg{R10, bytes}, current.dst, current.type});
        }
    } else if (obj.type == Byte && isOneBytesImm(obj.src)) {
        Imm *src = std::get_if<Imm>(&obj.src);
        src->value = src->value % 256;
    } else if (obj.type == Longword && isFourBytesImm(obj.src)) {
        // GCC throws a warning then we directly use MOVL to truncate an immediate value from 64 to 32 bits
        // TODO: Just modulo 256 the source operand
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{ current.src, Reg{ R10, 8 }, Quadword });
        it = asmList.emplace(std::next(it), Mov{ Reg{ R10, 4 }, current.dst, Longword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessMovsx(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Movsx>(*it);

    // MOVSLQ's source register always 4 bytes, the destination is 8 bytes
    /*
    if (auto r = std::get_if<Reg>(&obj.src)) {
        Movsx new_movsx = obj;
        new_movsx.src = Reg{ r->reg, 4 };
        *it = new_movsx;
    }
    if (auto r = std::get_if<Reg>(&obj.dst)) {
        Movsx new_movsx = obj;
        new_movsx.dst = Reg{ r->reg, 8 };
        *it = new_movsx;
    }
    */

    // MOVSX instruction can't use memory address as destination
    // or an immediate value as a source
    if (std::holds_alternative<Imm>(obj.src) || isMemoryAddress(obj.dst)) {
        auto current = obj;
        uint8_t src_bytes = GetBytesOfWordType(current.src_type);
        uint8_t dst_bytes = GetBytesOfWordType(current.dst_type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{ current.src, Reg{ R10, src_bytes }, current.src_type });
        it = asmList.emplace(std::next(it), Movsx{
            Reg{ R10, src_bytes },
            Reg{ R11, dst_bytes },
            current.src_type,
            current.dst_type
        });
        it = asmList.emplace(std::next(it), Mov{ Reg{ R11, dst_bytes }, current.dst, current.dst_type });
    }

    return std::next(it);
}

static std::list<Instruction>::iterator postprocessMovZeroExtend(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<MovZeroExtend>(*it);

    if (obj.src_type == Byte
        && (std::holds_alternative<Imm>(obj.src) || !std::holds_alternative<Reg>(obj.dst))) {
        auto current = obj;
        uint8_t dst_bytes = GetBytesOfWordType(current.dst_type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{ R10, 1 }, Byte});
        it = asmList.emplace(std::next(it), MovZeroExtend{ Reg{ R10, 1 }, Reg{ R11, dst_bytes }, Byte, obj.dst_type });
        it = asmList.emplace(std::next(it), Mov{ Reg{ R11, dst_bytes }, current.dst, obj.dst_type });
        return std::next(it);
    }

    if (obj.src_type == Longword) {
        if (auto r = std::get_if<Reg>(&obj.dst)) {
            *it = Mov{ obj.src, Reg{ r->reg, 4 }, Longword };
        } else if (isMemoryAddress(obj.dst)) {
            auto current = obj;
            it = asmList.erase(it);
            it = asmList.emplace(it, Mov{ current.src, Reg{ R11, 4 }, Longword });
            it = asmList.emplace(std::next(it), Mov{ Reg{ R11, 8 }, current.dst, Quadword });
        }
        return std::next(it);
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessLea(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Lea>(*it);
    // The source can't be a constant or a register - currently it's guaranteed.
    // The destination of LEA must be a register.
    if (!std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Lea{ current.src, Reg{ AX, 8 } });
        it = asmList.emplace(std::next(it), Mov{ Reg{ AX, 8 }, current.dst, Quadword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCvttsd2si(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cvttsd2si>(*it);
    // The destination of cvttsd2si must be a general purpose register
    if (!std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Cvttsd2si{ current.src, Reg{ AX, bytes }, current.type });
        it = asmList.emplace(std::next(it), Mov{ Reg{ AX, bytes }, current.dst, current.type });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCvtsi2sd(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cvtsi2sd>(*it);
    // The source of cvtsi2sdcan’t be a constant, and the destination must be a register
    if (std::holds_alternative<Imm>(obj.src) || !std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{ R10, bytes }, current.type} );
        it = asmList.emplace(std::next(it), Cvtsi2sd{ Reg{ R10, bytes }, Reg{ XMM15, bytes }, current.type });
        it = asmList.emplace(std::next(it), Mov{ Reg{ XMM15, bytes }, current.dst, Doubleword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCmp(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cmp>(*it);
    if (obj.type == Doubleword && !std::holds_alternative<Reg>(obj.rhs)) {
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.rhs, Reg{XMM15, 8}, Doubleword});
        it = asmList.emplace(std::next(it), Cmp{current.lhs, Reg{XMM15, 8}, Doubleword});
    } else if (std::holds_alternative<Imm>(obj.rhs)
        && obj.type == WordType::Quadword) {
        // TODO: Implement this complex case in a nicer way
        // Both rule applies at once
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Mov{current.rhs, Reg{R11, bytes}, current.type});
        it = asmList.emplace(std::next(it), Cmp{Reg{R10, bytes}, Reg{R11, bytes}, current.type});
    } else if ((isMemoryAddress(obj.lhs) && isMemoryAddress(obj.rhs))
        || obj.type == WordType::Quadword) {
        // CMP instruction can't have memory addresses both in source and destination
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Cmp{Reg{R10, bytes}, current.rhs, current.type});
    } else if (std::holds_alternative<Imm>(obj.rhs)) {
        // The second operand of CMP can't be a constant
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
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
        *it = SetCC{ obj.cond_code, Reg{ r->reg, 1 } };
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessPush(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Push>(*it);
    if (isEightBytesImm(obj.op)) {
        // PUSHQ can handle quadwords only
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{ current.op, Reg{ R10, 8 }, Quadword });
        it = asmList.emplace(std::next(it), Push{ Reg{ R10, 8 } });
    } else if (isXMMRegister(obj.op)) {
        // Can't have XMM register as operand
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Binary{ Sub_AB, Imm{ 8 }, Reg{ SP, 8 }, Quadword });
        it = asmList.emplace(std::next(it), Mov{ obj.op, Memory{ SP, 0 }, Doubleword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessBinary(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Binary>(*it);
    if (obj.type == Doubleword
        && (obj.op == Add_AB
            || obj.op == Sub_AB
            || obj.op == Mult_AB
            || obj.op == DivDouble_AB
            || obj.op == BWXor_AB)
        && (!isMemoryAddress(obj.src) || !std::holds_alternative<Reg>(obj.dst))) {
        // The destination of these has to be a register
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
        it = asmList.emplace(std::next(it), Mov{current.dst, Reg{XMM15, 8}, Doubleword});
        it = asmList.emplace(std::next(it), Binary{current.op, Reg{XMM14, 8}, Reg{XMM15, 8}, Doubleword});
        it = asmList.emplace(std::next(it), Mov{Reg{XMM15, 8}, current.dst, Doubleword});
    } else if (obj.type == Doubleword && (obj.op == BWAnd_AB || obj.op == BWOr_AB)) {
        // These instructions can't have memory addresses both in source and destination
        // AND and OR can't handle immediate values that can't fit into an int
        auto current = obj;
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
        it = asmList.emplace(std::next(it), Binary{current.op, Reg{XMM14, 8}, current.dst, Doubleword});
    } else if (obj.op == Add_AB
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
            uint8_t bytes = GetBytesOfWordType(current.type);
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
            uint8_t bytes = GetBytesOfWordType(current.type);
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
            uint8_t bytes = GetBytesOfWordType(current.type);
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
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Idiv{Reg{R10, bytes}, current.type});
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessDiv(std::list<Instruction> &asmList, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Div>(*it);
    // DIV can't have constant operand
    if (std::holds_alternative<Imm>(obj.src)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asmList.erase(it);
        it = asmList.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asmList.emplace(std::next(it), Div{Reg{R10, bytes}, current.type});
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
            else if constexpr (std::is_same_v<T, MovZeroExtend>)
                return postprocessMovZeroExtend(asmList, it);
            else if constexpr (std::is_same_v<T, Lea>)
                return postprocessLea(asmList, it);
            else if constexpr (std::is_same_v<T, Cvttsd2si>)
                return postprocessCvttsd2si(asmList, it);
            else if constexpr (std::is_same_v<T, Cvtsi2sd>)
                return postprocessCvtsi2sd(asmList, it);
            else if constexpr (std::is_same_v<T, Cmp>)
                return postprocessCmp(asmList, it);
            else if constexpr (std::is_same_v<T, SetCC>)
                return postprocessSetCC(asmList, it);
            else if constexpr (std::is_same_v<T, Push>)
                return postprocessPush(asmList, it);
            else if constexpr (std::is_same_v<T, Binary>)
                return postprocessBinary(asmList, it);
            else if constexpr (std::is_same_v<T, Idiv>)
                return postprocessIdiv(asmList, it);
            else if constexpr (std::is_same_v<T, Div>)
                return postprocessDiv(asmList, it);
            else if constexpr (std::is_same_v<T, Function>) {
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
