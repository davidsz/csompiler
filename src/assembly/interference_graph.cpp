#include "interference_graph.h"
#include <cassert>
#include <map>

namespace assembly {

static std::map<const Instruction *, std::set<Register>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<Register>> s_blockAnnotations;
static size_t s_exitId = 0;

// Don't include SP and BP (they manage the stack frame);
// R10 and R11 are reserved for the instruction fixup phase
static const std::vector<Register> s_integerRegisters = {
    AX, BX, CX, DX, DI, SI, R8, R9, R12, R13, R14, R15
};

template <typename Fn>
static void ForEachOperand(const Instruction &instr, Fn &&fn)
{
    std::visit([&](const auto &i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Comment>) {
        } else if constexpr (std::is_same_v<T, Mov>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Movsx>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Lea>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Ret>) {
        } else if constexpr (std::is_same_v<T, Unary>) {
            fn(i.src);
        } else if constexpr (std::is_same_v<T, Binary>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Idiv>) {
            fn(i.src);
        } else if constexpr (std::is_same_v<T, Div>) {
            fn(i.src);
        } else if constexpr (std::is_same_v<T, Cdq>) {
        } else if constexpr (std::is_same_v<T, Cmp>) {
            fn(i.lhs);
            fn(i.rhs);
        } else if constexpr (std::is_same_v<T, Jmp>) {
        } else if constexpr (std::is_same_v<T, JmpCC>) {
        } else if constexpr (std::is_same_v<T, SetCC>) {
            fn(i.op);
        } else if constexpr (std::is_same_v<T, Label>) {
        } else if constexpr (std::is_same_v<T, Push>) {
            fn(i.op);
        } else if constexpr (std::is_same_v<T, Pop>) {
        } else if constexpr (std::is_same_v<T, Call>) {
        }
    }, instr);
}

static std::map<GraphKey, GraphData> buildBaseGraph(const std::vector<Register> &registers)
{
    std::map<GraphKey, GraphData> base_graph;

    for (const auto &reg : registers)
        base_graph.emplace(reg, GraphData{ });

    for (size_t i = 0; i < registers.size(); ++i) {
        for (size_t j = 0; j < registers.size(); ++j) {
            if (i == j)
                continue;
            GraphKey a = registers[i];
            GraphKey b = registers[j];
            base_graph[a].neighbors.insert(b);
            base_graph[b].neighbors.insert(a);
        }
    }

    return base_graph;
}

static void addPseudoRegisters(
    std::list<CFGBlock> &blocks,
    std::map<GraphKey, GraphData> &graph)
{
    for (auto &block : blocks) {
        for (auto &instruction : block.instructions) {
            ForEachOperand(instruction, [&graph](const Operand &operand) {
                if (const Pseudo *pseudo = std::get_if<Pseudo>(&operand))
                    graph.emplace(pseudo->name, GraphData{ });
                // TODO: PseudoAggregate
            });
        }
    }
}

static inline void connect(CFGBlock *from, CFGBlock *to)
{
    from->successors.insert(to);
    to->predecessors.insert(from);
}

static void addControlFlowEdges(std::list<CFGBlock> &blocks)
{
    // Map labels to their blocks
    std::map<std::string, CFGBlock *> blockLabels;
    for (auto &block : blocks) {
        block.predecessors.clear();
        block.successors.clear();
        if (block.instructions.empty())
            continue;
        if (const Label *label = std::get_if<Label>(&block.instructions.front()))
            blockLabels[label->identifier] = &block;
    }

    // Connect blocks
    CFGBlock *entry_block = &blocks.front();
    CFGBlock *exit_block = &blocks.back();
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
        CFGBlock *block = &*it;
        CFGBlock *next_block = (block == exit_block)
            ? exit_block : &*std::next(it);
        if (block == entry_block || block->instructions.empty()) {
            connect(block, next_block);
            continue;
        }
        Instruction &last = block->instructions.back();
        if (std::holds_alternative<Ret>(last))
            connect(block, exit_block);
        else if (const Jmp *j = std::get_if<Jmp>(&last)) {
            if (CFGBlock *target = blockLabels[j->identifier])
                connect(block, target);
        } else if (const JmpCC *jcc = std::get_if<JmpCC>(&last)) {
            if (CFGBlock *target = blockLabels[jcc->identifier])
                connect(block, target);
            connect(block, next_block);
        } else
            connect(block, next_block);
    }
}

static
std::pair<std::vector<Operand>, std::vector<Operand>> findUsedAndUpdated(
    const Instruction &instr,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    return std::visit([&](const auto &i) -> std::pair<std::vector<Operand>, std::vector<Operand>> {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Comment>) {
        } else if constexpr (std::is_same_v<T, Mov>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Movsx>) {
        } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
        } else if constexpr (std::is_same_v<T, Lea>) {
        } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
        } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
        } else if constexpr (std::is_same_v<T, Ret>) {
        } else if constexpr (std::is_same_v<T, Unary>) {
            return { { i.src }, { i.src } };
        } else if constexpr (std::is_same_v<T, Binary>) {
            return { { i.src, i.dst }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Idiv>) {
            return { { i.src, Reg{ AX }, Reg{ DX } }, { Reg{ AX }, Reg{ DX } } };
        } else if constexpr (std::is_same_v<T, Div>) {
        } else if constexpr (std::is_same_v<T, Cdq>) {
            return { { Reg{ AX } }, { Reg{ DX } } };
        } else if constexpr (std::is_same_v<T, Cmp>) {
            return { { i.lhs, i.rhs }, { } };
        } else if constexpr (std::is_same_v<T, Jmp>) {
        } else if constexpr (std::is_same_v<T, JmpCC>) {
        } else if constexpr (std::is_same_v<T, SetCC>) {
            return { { }, { i.op } };
        } else if constexpr (std::is_same_v<T, Label>) {
        } else if constexpr (std::is_same_v<T, Push>) {
            return { { i.op }, { } };
        } else if constexpr (std::is_same_v<T, Pop>) {
        } else if constexpr (std::is_same_v<T, Call>) {
            const FunEntry *entry = asm_symbol_table->getAs<FunEntry>(i.identifier);
            assert(entry);
            std::vector<Operand> used;
            for (Register reg : entry->arg_registers)
                used.push_back(Reg{ reg });
            return {
                std::move(used),
                { Reg{ DI }, Reg{ SI }, Reg{ DX }, Reg{ CX }, Reg{ R8 }, Reg{ R9 }, Reg{ AX } }
            };
        }
        return {};
    }, instr);
}

// Transfer function
static void transfer(
    const CFGBlock *block,
    const std::set<Register> &end_live_registers,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    std::set<Register> current_live_registers = end_live_registers;
    for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
        const Instruction &instruction = *it;
        s_instructionAnnotations[&instruction] = current_live_registers;
        auto [used, updated] = findUsedAndUpdated(instruction, asm_symbol_table);
        for (auto &v : updated) {
            if (const Reg *reg = std::get_if<Reg>(&v))
                current_live_registers.erase(reg->reg);
        }
        for (auto &v : used) {
            if (const Reg *reg = std::get_if<Reg>(&v))
                current_live_registers.insert(reg->reg);
        }
    }
    s_blockAnnotations[block] = std::move(current_live_registers);
}

// Meet operator: propagates information about live registers
// from one block to another.
static std::set<Register> meet(
    const CFGBlock *block)
{
    std::set<Register> live_registers;
    for (auto &succ : block->successors) {
        if (succ->id == 0)
            assert(false);
        else if (succ->id == s_exitId) {
            live_registers.insert(AX);
        } else {
            const std::set<Register> &succ_live_regs = s_blockAnnotations[succ];
            live_registers.insert(succ_live_regs.begin(), succ_live_regs.end());
        }
    }
    return live_registers;
}

// Iterative algorithm: implements a backward (liveness) analysis
static void findLiveRegisters(
    const std::list<CFGBlock> &blocks,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    // TODO: Can be optimized by postordering
    std::list<const CFGBlock *> worklist;
    for (auto &block : blocks) {
        if (block.id == 0 || block.id == s_exitId)
            continue;
        worklist.push_back(&block);
        s_blockAnnotations[&block] = {};
    }

    while (!worklist.empty()) {
        const CFGBlock *block = worklist.front();
        worklist.pop_front();
        std::set<Register> old_annotations = s_blockAnnotations[block];
        std::set<Register> end_live = meet(block);
        transfer(block, end_live, asm_symbol_table);
        if (old_annotations != s_blockAnnotations[block]) {
            for (auto pred : block->predecessors) {
                if (pred->id == 0 || pred->id == s_exitId)
                    continue;
                if (std::find(worklist.begin(), worklist.end(), pred) == worklist.end())
                    worklist.push_back(pred);
            }
        }
    }
}

static void addInterferenceEdges(
    std::list<CFGBlock> &blocks,
    std::map<GraphKey, GraphData> &interference_graph,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    for (auto &block : blocks) {
        if (block.id == 0 || block.id == s_exitId)
            continue;
        for (auto &instr : block.instructions) {
            auto [used, updated] = findUsedAndUpdated(instr, asm_symbol_table);
            const std::set<Register> &live_registers = s_instructionAnnotations[&instr];
            for (const Register &live_reg : live_registers) {
                if (const Mov *mov = std::get_if<Mov>(&instr)) {
                    if (live_reg == mov->src)
                        continue;
                }
                for (auto &u : updated) {
                    std::optional<GraphKey> updated_key;
                    if (const Reg *updated_reg = std::get_if<Reg>(&u))
                        updated_key = updated_reg->reg;
                    if (const Pseudo *updated_pseudo = std::get_if<Pseudo>(&u))
                        updated_key = updated_pseudo->name;
                    if (!updated_key)
                        continue;
                    if (interference_graph.contains(live_reg)
                        && interference_graph.contains(*updated_key)
                        && live_reg != u) {
                        interference_graph[live_reg].neighbors.insert(*updated_key);
                        interference_graph[*updated_key].neighbors.insert(live_reg);
                    }
                }
            }
        }
    }
}

std::map<GraphKey, GraphData> buildInterferenceGraph(
    std::list<CFGBlock> &blocks,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table)
{
    // A fully connected base graph of the physical registers
    std::map<GraphKey, GraphData> interference_graph = buildBaseGraph(s_integerRegisters);
    // Extend it with pseudo-registers
    addPseudoRegisters(blocks, interference_graph);
    // Finish the control flow graph before performing liveness analysis
    addControlFlowEdges(blocks);

    // Liveness analysis
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    s_exitId = blocks.back().id;
    findLiveRegisters(blocks, asm_symbol_table);

    // We add edges to the interference graph using the liveness information
    addInterferenceEdges(blocks, interference_graph, asm_symbol_table);

    return interference_graph;
}

} // assembly
