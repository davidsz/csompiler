#include "asm_nodes.h"
#include "asm_printer_utils.h"
#include "asm_symbol_table.h"
#include <cassert>
#include <limits>
#include <map>
#include <list>

namespace assembly {

static inline bool replacePseudo(
    Operand &op,
    const std::map<std::string, Register> &register_map,
    ASMSymbolTable *asm_symbol_table)
{
    return std::visit([&](auto &obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Pseudo>) {
            if (auto it = register_map.find(obj.name); it != register_map.end()) {
                ObjEntry *entry = asm_symbol_table->getAs<ObjEntry>(obj.name);
                assert(entry);
                assert(!entry->is_static);
                std::cout << "----- replace " << obj.name << " with ";
                std::cout << getEightByteName(it->second);
                std::cout << " (" << entry->type.size() << " bytes)" << std::endl;
                op.emplace<Reg>(it->second, entry->type.size());
                return true;
            }
        }
        return false;
    }, op);
}

static inline std::list<Instruction>::iterator removeIfNeeded(
    std::list<Instruction> &instructions,
    std::list<Instruction>::iterator it,
    const Operand &a,
    const Operand &b,
    bool changed)
{
    if (!changed)
        return std::next(it);
    const Reg *ra = std::get_if<Reg>(&a);
    const Reg *rb = std::get_if<Reg>(&b);
    if (ra && rb && ra->reg == rb->reg && ra->bytes == rb->bytes) {
        std::cout << "---- Removing instruction with duplicate register: " << getEightByteName(ra->reg) << std::endl;
        return instructions.erase(it);
    }
    return std::next(it);
}

void replacePseudoRegisters(
    std::list<CFGBlock> &blocks,
    const std::map<std::string, Register> &reg_map,
    FunEntry *function_entry,
    ASMSymbolTable *asm_symbol_table)
{
    std::cout << "Replace pseudoregisters in a function" << std::endl;

    std::set<Register> &callee_saved_registers = function_entry->callee_saved_registers;
    if (!callee_saved_registers.empty()) {
        CFGBlock &first_block = blocks.front();
        for (const Register &reg : callee_saved_registers)
            first_block.instructions.emplace_front(Push{ Reg{ reg, 8 } });
        first_block.instructions.emplace_front(Comment{ "Pushing callee-saved registers" });
    }

    for (auto &block : blocks) {
        for (auto it = block.instructions.begin(); it != block.instructions.end();) {
            it = std::visit([&](auto &obj) {
                bool changed = false;
                using T = std::decay_t<decltype(obj)>;
                if constexpr (std::is_same_v<T, Mov>) {
                    std::cout << "--- Mov " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, Movsx>) {
                    std::cout << "--- Movsx " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
                    std::cout << "--- MovZeroExtend " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, Lea>) {
                    std::cout << "--- Lea " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
                    std::cout << "--- Cvttsd2si " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
                    std::cout << "--- Cvtsi2sd " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Unary>) {
                    std::cout << "--- Unary " << std::endl;
                    replacePseudo(obj.src, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Binary>) {
                    std::cout << "--- Binary " << std::endl;
                    changed |= replacePseudo(obj.src, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.dst, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Idiv>) {
                    std::cout << "--- Idiv " << std::endl;
                    replacePseudo(obj.src, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Div>) {
                    std::cout << "--- Div " << std::endl;
                    replacePseudo(obj.src, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Cmp>) {
                    std::cout << "--- Cmp " << std::endl;
                    changed |= replacePseudo(obj.lhs, reg_map, asm_symbol_table);
                    changed |= replacePseudo(obj.rhs, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, SetCC>) {
                    std::cout << "--- SetCC " << std::endl;
                    replacePseudo(obj.op, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Push>) {
                    std::cout << "--- Push " << std::endl;
                    replacePseudo(obj.op, reg_map, asm_symbol_table);
                } else if constexpr (std::is_same_v<T, Ret>) {
                    for (const Register &reg : callee_saved_registers)
                        block.instructions.emplace(it, Pop{ reg });
                }
                return std::next(it);
            }, *it);
        }
    }
}

// Replace each pseudo-register with proper stack offsets or static variables;
// calculates the overall stack size needed to store all local variables.
static int postprocessPseudoRegisters(
    std::list<CFGBlock> &blocks,
    int stack_start,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    std::map<std::string, int> pseudo_offset;
    int current_offset = stack_start;

    auto resolvePseudo = [&](Operand &op) {
        std::string name;
        size_t extra_offset = 0; // The offset inside the array
        if (auto pseudo = std::get_if<Pseudo>(&op))
            name = pseudo->name;
        else if (auto pseudo_aggr = std::get_if<PseudoAggregate>(&op)) {
            name = pseudo_aggr->name;
            extra_offset = pseudo_aggr->offset;
        } else
            return;

        ObjEntry *entry = asm_symbol_table->getAs<ObjEntry>(name);
        assert(entry);
        if (entry->is_static) {
            // Replace static variables with Data operands
            op.emplace<Data>(name, extra_offset);
            return;
        }
        // All other variable types are stack offsets
        // If a variable has no stack offset yet, determine it
        if (auto it = pseudo_offset.find(name); it == pseudo_offset.end()) {
            size_t size = entry->type.size();
            size_t align = entry->type.alignment();
            if (size >= 16)
                align = std::max<size_t>(align, 16);
            current_offset -= static_cast<int>(size);
            current_offset &= static_cast<int>(~(align - 1));
            pseudo_offset[name] = current_offset;
        }
        op.emplace<Memory>(BP, pseudo_offset[name] + static_cast<int>(extra_offset));
    };

    for (auto &block : blocks) {
        for (auto &inst : block.instructions) {
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
    }

    return -current_offset;
}

void postprocessPseudoRegisters(
    std::list<TopLevel> &asm_list,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    for (auto &inst : asm_list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                // If the function returns on the stack, the pointer
                // to the returned data is at -8(%rbp); allocate space
                // for this pointer.
                int stack_start = 0;
                FunEntry *entry = asm_symbol_table->getAs<FunEntry>(obj.name);
                assert(entry);
                if (entry->return_on_stack)
                    stack_start = -8;

                // Locals variables
                int locals_size = postprocessPseudoRegisters(obj.blocks, stack_start, asm_symbol_table);
                // Callee-saved registers pushed to the stack
                int callee_saved_bytes = 8 * static_cast<int>(entry->callee_saved_registers.size());

                std::cout << "Function " << obj.name << " local size: " << locals_size << std::endl;
                std::cout << "Function " << obj.name << " callee_saved size: " << callee_saved_bytes << std::endl;

                int total_stack_bytes = locals_size + callee_saved_bytes;
                int adjusted_stack_bytes = (total_stack_bytes + 15) & ~15;
                obj.stack_size = adjusted_stack_bytes - callee_saved_bytes;
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

static bool immLongerThanOneByte(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op))
        return static_cast<int64_t>(imm->value) > std::numeric_limits<int8_t>::max();
    return false;
}

// "The assembler permits an immediate value in addq, imulq, subq, cmpq, or pushq only if
// it can be represented as a signed 32-bit integer. That’s because these instructions all
// sign extend their immediate operands from 32 to 64 bits. If an immediate value can
// be represented in 32 bits only as an unsigned integer—which implies that its upper
// bit is set—sign extending it will change its value."
static bool immLongerThanFourByte(const Operand &op)
{
    // Also check for negative overflow
    if (const Imm *imm = std::get_if<Imm>(&op)) {
        int64_t v = static_cast<int64_t>(imm->value);
        return v < std::numeric_limits<int32_t>::lowest() ||
               v > std::numeric_limits<int32_t>::max();
    }
    return false;
}

static bool isXMMRegister(const Operand &op)
{
    if (const Reg *r = std::get_if<Reg>(&op))
        return r->reg >= XMM0 && r->reg <= XMM15;
    return false;
}

static std::list<Instruction>::iterator postprocessMov(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Mov>(*it);
    // MOV instruction can't have memory addresses both in source and destination
    if ((isMemoryAddress(obj.src) && isMemoryAddress(obj.dst))
        || (obj.type == WordType::Quadword && std::holds_alternative<Imm>(obj.src) && isMemoryAddress(obj.dst))) {
        auto current = obj;
        it = asm_list.erase(it);
        if (current.type == WordType::Doubleword) {
            it = asm_list.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
            it = asm_list.emplace(std::next(it), Mov{Reg{XMM14, 8}, current.dst, Doubleword});
        } else {
            uint8_t bytes = GetBytesOfWordType(current.type);
            it = asm_list.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
            it = asm_list.emplace(std::next(it), Mov{Reg{R10, bytes}, current.dst, current.type});
        }
    } else if (obj.type == Byte && immLongerThanOneByte(obj.src)) {
        if (Imm *src = std::get_if<Imm>(&obj.src))
            src->value = src->value % 256;
    } else if (obj.type == Longword && immLongerThanFourByte(obj.src)) {
        // GCC throws a warning when we directly use MOVL to truncate an immediate value from 64 to 32 bits
        Imm *src = std::get_if<Imm>(&obj.src);
        src->value = int32_t(src->value);
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessMovsx(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Movsx>(*it);

    // MOVSX instruction can't use memory address as destination
    // or an immediate value as a source
    if (std::holds_alternative<Imm>(obj.src) || isMemoryAddress(obj.dst)) {
        auto current = obj;
        uint8_t src_bytes = GetBytesOfWordType(current.src_type);
        uint8_t dst_bytes = GetBytesOfWordType(current.dst_type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{ current.src, Reg{ R10, src_bytes }, current.src_type });
        it = asm_list.emplace(std::next(it), Movsx{
            Reg{ R10, src_bytes },
            Reg{ R11, dst_bytes },
            current.src_type,
            current.dst_type
        });
        it = asm_list.emplace(std::next(it), Mov{ Reg{ R11, dst_bytes }, current.dst, current.dst_type });
    }

    return std::next(it);
}

static std::list<Instruction>::iterator postprocessMovZeroExtend(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<MovZeroExtend>(*it);

    if (obj.src_type == Byte
        && (std::holds_alternative<Imm>(obj.src) || !std::holds_alternative<Reg>(obj.dst))) {
        auto current = obj;
        uint8_t dst_bytes = GetBytesOfWordType(current.dst_type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{ R10, 1 }, Byte});
        it = asm_list.emplace(std::next(it), MovZeroExtend{ Reg{ R10, 1 }, Reg{ R11, dst_bytes }, Byte, current.dst_type });
        it = asm_list.emplace(std::next(it), Mov{ Reg{ R11, dst_bytes }, current.dst, current.dst_type });
        return std::next(it);
    }

    if (obj.src_type == Longword) {
        if (auto r = std::get_if<Reg>(&obj.dst)) {
            *it = Mov{ obj.src, Reg{ r->reg, 4 }, Longword };
        } else if (isMemoryAddress(obj.dst)) {
            auto current = obj;
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{ current.src, Reg{ R11, 4 }, Longword });
            it = asm_list.emplace(std::next(it), Mov{ Reg{ R11, 8 }, current.dst, Quadword });
        }
        return std::next(it);
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessLea(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Lea>(*it);
    // The source can't be a constant or a register - currently it's guaranteed.
    // The destination of LEA must be a register.
    if (!std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Lea{ current.src, Reg{ AX, 8 } });
        it = asm_list.emplace(std::next(it), Mov{ Reg{ AX, 8 }, current.dst, Quadword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCvttsd2si(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cvttsd2si>(*it);
    // The destination of cvttsd2si must be a general purpose register
    if (!std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Cvttsd2si{ current.src, Reg{ AX, bytes }, current.type });
        it = asm_list.emplace(std::next(it), Mov{ Reg{ AX, bytes }, current.dst, current.type });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCvtsi2sd(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cvtsi2sd>(*it);
    // The source of cvtsi2sdcan’t be a constant, and the destination must be a register
    if (std::holds_alternative<Imm>(obj.src) || !std::holds_alternative<Reg>(obj.dst)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{ R10, bytes }, current.type} );
        it = asm_list.emplace(std::next(it), Cvtsi2sd{ Reg{ R10, bytes }, Reg{ XMM15, bytes }, current.type });
        it = asm_list.emplace(std::next(it), Mov{ Reg{ XMM15, bytes }, current.dst, Doubleword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessCmp(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Cmp>(*it);
    if (obj.type == Doubleword && !std::holds_alternative<Reg>(obj.rhs)) {
        auto current = obj;
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.rhs, Reg{XMM15, 8}, Doubleword});
        it = asm_list.emplace(std::next(it), Cmp{current.lhs, Reg{XMM15, 8}, Doubleword});
    } else if (std::holds_alternative<Imm>(obj.rhs)
        && obj.type == WordType::Quadword) {
        // TODO: Implement this complex case in a nicer way
        // Both rule applies at once
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Mov{current.rhs, Reg{R11, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Cmp{Reg{R10, bytes}, Reg{R11, bytes}, current.type});
    } else if ((isMemoryAddress(obj.lhs) && isMemoryAddress(obj.rhs))
        || obj.type == WordType::Quadword) {
        // CMP instruction can't have memory addresses both in source and destination
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.lhs, Reg{R10, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Cmp{Reg{R10, bytes}, current.rhs, current.type});
    } else if (std::holds_alternative<Imm>(obj.rhs)) {
        // The second operand of CMP can't be a constant
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.rhs, Reg{R11, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Cmp{current.lhs, Reg{R11, bytes}, current.type});
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

static std::list<Instruction>::iterator postprocessPush(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Push>(*it);
    if (immLongerThanFourByte(obj.op)) {
        // PUSHQ can handle quadwords only
        auto current = obj;
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{ current.op, Reg{ R10, 8 }, Quadword });
        it = asm_list.emplace(std::next(it), Push{ Reg{ R10, 8 } });
    } else if (isXMMRegister(obj.op)) {
        // Can't have XMM register as operand
        auto current = obj;
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Binary{ Sub_AB, Imm{ 8 }, Reg{ SP, 8 }, Quadword });
        it = asm_list.emplace(std::next(it), Mov{ obj.op, Memory{ SP, 0 }, Doubleword });
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessBinary(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
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
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
        it = asm_list.emplace(std::next(it), Mov{current.dst, Reg{XMM15, 8}, Doubleword});
        it = asm_list.emplace(std::next(it), Binary{current.op, Reg{XMM14, 8}, Reg{XMM15, 8}, Doubleword});
        it = asm_list.emplace(std::next(it), Mov{Reg{XMM15, 8}, current.dst, Doubleword});
    } else if (obj.type == Doubleword && (obj.op == BWAnd_AB || obj.op == BWOr_AB)) {
        // These instructions can't have memory addresses both in source and destination
        // AND and OR can't handle immediate values that can't fit into an int
        auto current = obj;
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{XMM14, 8}, Doubleword});
        it = asm_list.emplace(std::next(it), Binary{current.op, Reg{XMM14, 8}, current.dst, Doubleword});
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
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
            it = asm_list.emplace(std::next(it), Binary{current.op, Reg{R10, bytes}, current.dst, current.type});
        }
    } else if (obj.op == Mult_AB) {
        // IMUL can't use memory address as its destination
        // IMULQ can't handle immediate values that can't fit into an int
        // TODO: Second operand can't be constant?
        // TODO: Make it nicer
        if (obj.type == WordType::Quadword && isMemoryAddress(obj.dst)) {
            auto current = obj;
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{current.src, Reg{R10, 8}, current.type});
            it = asm_list.emplace(std::next(it), Mov{current.dst, Reg{R11, 8}, current.type});
            it = asm_list.emplace(std::next(it), Binary{current.op, Reg{R10, 8}, Reg{R11, 8}, WordType::Quadword});
            it = asm_list.emplace(std::next(it), Mov{Reg{R11, 8}, current.dst, current.type});
        } else if (isMemoryAddress(obj.dst)) {
            auto current = obj;
            uint8_t bytes = GetBytesOfWordType(current.type);
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{current.dst, Reg{R11, bytes}, current.type});
            it = asm_list.emplace(std::next(it), Binary{current.op, current.src, Reg{R11, bytes}, current.type});
            it = asm_list.emplace(std::next(it), Mov{Reg{R11, bytes}, current.dst, current.type});
        } else if (obj.type == WordType::Quadword) {
            auto current = obj;
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{current.src, Reg{R10, 8}, WordType::Quadword});
            it = asm_list.emplace(std::next(it), Binary{current.op, Reg{R10, 8}, current.dst, WordType::Quadword});
        }
    } else if (obj.op == ShiftL_AB || obj.op == ShiftRU_AB || obj.op == ShiftRS_AB) {
        // SHL, SHR and SAR can only have constant or CL register on their left (count)
        if (isMemoryAddress(obj.src)) {
            auto current = obj;
            uint8_t bytes = GetBytesOfWordType(current.type);
            it = asm_list.erase(it);
            it = asm_list.emplace(it, Mov{current.src, Reg{CX, bytes}, current.type});
            it = asm_list.emplace(std::next(it), Binary{current.op, Reg{CX, 1}, current.dst, current.type});
        }
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessIdiv(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Idiv>(*it);
    // IDIV can't have constant operand
    if (std::holds_alternative<Imm>(obj.src)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Idiv{Reg{R10, bytes}, current.type});
    }
    return std::next(it);
}

static std::list<Instruction>::iterator postprocessDiv(std::list<Instruction> &asm_list, std::list<Instruction>::iterator it)
{
    auto &obj = std::get<Div>(*it);
    // DIV can't have constant operand
    if (std::holds_alternative<Imm>(obj.src)) {
        auto current = obj;
        uint8_t bytes = GetBytesOfWordType(current.type);
        it = asm_list.erase(it);
        it = asm_list.emplace(it, Mov{current.src, Reg{R10, bytes}, current.type});
        it = asm_list.emplace(std::next(it), Div{Reg{R10, bytes}, current.type});
    }
    return std::next(it);
}

static void postprocessInvalidInstructions(std::list<Instruction> &asm_list)
{
    for (auto it = asm_list.begin(); it != asm_list.end();) {
        it = std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Mov>)
                return postprocessMov(asm_list, it);
            else if constexpr (std::is_same_v<T, Movsx>)
                return postprocessMovsx(asm_list, it);
            else if constexpr (std::is_same_v<T, MovZeroExtend>)
                return postprocessMovZeroExtend(asm_list, it);
            else if constexpr (std::is_same_v<T, Lea>)
                return postprocessLea(asm_list, it);
            else if constexpr (std::is_same_v<T, Cvttsd2si>)
                return postprocessCvttsd2si(asm_list, it);
            else if constexpr (std::is_same_v<T, Cvtsi2sd>)
                return postprocessCvtsi2sd(asm_list, it);
            else if constexpr (std::is_same_v<T, Cmp>)
                return postprocessCmp(asm_list, it);
            else if constexpr (std::is_same_v<T, SetCC>)
                return postprocessSetCC(asm_list, it);
            else if constexpr (std::is_same_v<T, Push>)
                return postprocessPush(asm_list, it);
            else if constexpr (std::is_same_v<T, Binary>)
                return postprocessBinary(asm_list, it);
            else if constexpr (std::is_same_v<T, Idiv>)
                return postprocessIdiv(asm_list, it);
            else if constexpr (std::is_same_v<T, Div>)
                return postprocessDiv(asm_list, it);
            else if constexpr (std::is_same_v<T, Function>) {
                for (auto &block : obj.blocks)
                    postprocessInvalidInstructions(block.instructions);
                return std::next(it);
            } else
                return std::next(it);
        }, *it);
    }
}

void postprocessInvalidInstructions(std::list<TopLevel> &asm_list)
{
    for (auto &top_level_obj : asm_list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                for (auto &block : obj.blocks)
                    postprocessInvalidInstructions(block.instructions);
            }
        }, top_level_obj);
    }
}

}; // namespace assembly
