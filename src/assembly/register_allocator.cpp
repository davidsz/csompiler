#include "asm_nodes.h"
#include "asm_symbol_table.h"
#include "asm_printer_utils.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <numeric>
#include <ranges>

namespace assembly {

// postprocess.cpp
void replacePseudoRegisters(
    std::list<CFGBlock> &blocks,
    const std::map<std::string, Register> &register_map,
    const std::set<Register> &callee_saved_registers);

// Don't include SP and BP (they manage the stack frame);
// R10 and R11 are reserved for the instruction fixup phase
static const std::vector<Register> s_integerRegisters = {
    AX, BX, CX, DX, DI, SI, R8, R9, R12, R13, R14, R15
};

// XMM14 and XMM15 are reserved for the instruction fixup phase
static const std::vector<Register> s_floatingPointRegisters = {
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13
};

// Registers and PseudoRegisters
using GraphKey = std::variant<Register, std::string>;

struct GraphData {
    std::set<GraphKey> neighbors = {};
    double spill_cost = 0;
    size_t color = 0;
    bool pruned = false;
};

static std::map<const Instruction *, std::set<GraphKey>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<GraphKey>> s_blockAnnotations;
static size_t s_exitId = 0;

static const std::set<Register> s_allCalleeSavedRegisters = {
    BX, BP, R12, R13, R14, R15
};

static std::optional<GraphKey> operandToKey(const Operand &operand)
{
    if (const Reg *r = std::get_if<Reg>(&operand))
        return r->reg;
    if (const Pseudo *p = std::get_if<Pseudo>(&operand))
        return p->name;
    if (const PseudoAggregate *p = std::get_if<PseudoAggregate>(&operand))
        return p->name;
    return std::nullopt;
}

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

    for (const auto &reg : registers) {
        // These are physical registers, we already know their spill costs
        base_graph.emplace(reg, GraphData{
            .spill_cost = std::numeric_limits<double>::max()
        });
    }

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

static inline void addPseudoRegisters(
    std::list<CFGBlock> &blocks,
    std::map<GraphKey, GraphData> &graph,
    bool processing_floating_points,
    const std::set<std::string> &aliased_vars,
    ASMSymbolTable *asm_symbol_table)
{
    // TODO: Variables in loops can have much bigger spill costs
    for (auto &block : blocks) {
        for (auto &instruction : block.instructions) {
            ForEachOperand(instruction, [&](const Operand &operand) {
                if (const Pseudo *pseudo = std::get_if<Pseudo>(&operand)) {
                    if (aliased_vars.contains(pseudo->name))
                        return;
                    ObjEntry *entry = asm_symbol_table->getAs<ObjEntry>(pseudo->name);
                    assert(entry);
                    if (entry->is_static)
                        return;
                    if (processing_floating_points != entry->type.isWord(Doubleword))
                        return;
                    auto [it, inserted] = graph.try_emplace(pseudo->name, GraphData{ });
                    it->second.spill_cost += 1.0;
                }
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

static void collectReadRegs(const Operand &op, std::vector<Operand> &out)
{
    std::visit([&](const auto &obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Reg> || std::is_same_v<T, Pseudo>) {
            out.push_back(op);
        } else if constexpr (std::is_same_v<T, Memory>) {
            // mov (%rax), %ebx -> %rax have been read for addressing
            out.push_back(Reg{ obj.reg, 8 }); // Base register is always 8 bytes
        } else if constexpr (std::is_same_v<T, Indexed>) {
            // mov (%rax, %rcx, 4), %ebx -> rax and rcx have been read
            out.push_back(Reg{ obj.base, 8 });
            out.push_back(Reg{ obj.index, 8 });
        }
    }, op);
}

static void collectUpdatedRegs(const Operand &op, std::vector<Operand> &read_out, std::vector<Operand> &write_out)
{
    std::visit([&](const auto& obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Reg> || std::is_same_v<T, Pseudo>) {
            write_out.push_back(op);
        } else if constexpr (std::is_same_v<T, Memory> || std::is_same_v<T, Indexed>) {
            // We don't change the register, we only read it
            collectReadRegs(op, read_out);
        }
    }, op);
}

static
std::pair<std::vector<Operand>, std::vector<Operand>> findUsedAndUpdated(
    const Instruction &instr,
    ASMSymbolTable *asm_symbol_table)
{
    auto [raw_used, raw_updated] = std::visit([&](const auto &i) -> std::pair<std::vector<Operand>, std::vector<Operand>> {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Comment>) {
        } else if constexpr (std::is_same_v<T, Mov>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Movsx>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Lea>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
            return { { i.src }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Ret>) {
        } else if constexpr (std::is_same_v<T, Unary>) {
            return { { i.src }, { i.src } };
        } else if constexpr (std::is_same_v<T, Binary>) {
            return { { i.src, i.dst }, { i.dst } };
        } else if constexpr (std::is_same_v<T, Idiv>) {
            return { { i.src, Reg{ AX }, Reg{ DX } }, { Reg{ AX }, Reg{ DX } } };
        } else if constexpr (std::is_same_v<T, Div>) {
            return { { i.src, Reg{ AX }, Reg{ DX } }, { Reg{ AX }, Reg{ DX } } };
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
                {
                    Reg{ DI }, Reg{ SI }, Reg{ DX }, Reg{ CX }, Reg{ R8 }, Reg{ R9 }, Reg{ AX },
                    Reg{ XMM0 }, Reg{ XMM1 }, Reg{ XMM2 }, Reg{ XMM3 }, Reg{ XMM4 }, Reg{ XMM5 },
                    Reg{ XMM6 }, Reg{ XMM7 }, Reg{ XMM8 }, Reg{ XMM9 }, Reg{ XMM10 }, Reg{ XMM11 },
                    Reg{ XMM12 }, Reg{ XMM13 }, Reg{ XMM14 }, Reg{ XMM15 }
                }
            };
        }
        return {};
    }, instr);

    std::vector<Operand> final_used;
    for (const auto& op : raw_used)
        collectReadRegs(op, final_used);
    std::vector<Operand> final_updated;
    for (const auto& op : raw_updated)
        collectUpdatedRegs(op, final_used, final_updated);
    return { std::move(final_used), std::move(final_updated) };
}

// Transfer function
static void transfer(
    const CFGBlock *block,
    const std::set<GraphKey> &end_live_registers,
    ASMSymbolTable *asm_symbol_table)
{
    std::set<GraphKey> current_live_registers = end_live_registers;
    for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
        const Instruction &instruction = *it;
        s_instructionAnnotations[&instruction] = current_live_registers;
        auto [used, updated] = findUsedAndUpdated(instruction, asm_symbol_table);
        for (auto &v : updated) {
            if (auto key = operandToKey(v))
                current_live_registers.erase(*key);
        }
        for (auto &v : used) {
            if (auto key = operandToKey(v))
                current_live_registers.insert(*key);
        }
    }
    s_blockAnnotations[block] = std::move(current_live_registers);
}

// Meet operator: propagates information about live registers
// from one block to another.
static std::set<GraphKey> meet(
    const CFGBlock *block,
    FunEntry *function_entry)
{
    std::set<GraphKey> live_registers;
    for (auto &succ : block->successors) {
        if (succ->id == 0)
            assert(false);
        else if (succ->id == s_exitId) {
            for (Register reg : function_entry->ret_registers)
                live_registers.insert(reg);
        } else {
            const std::set<GraphKey> &succ_live_regs = s_blockAnnotations[succ];
            live_registers.insert(succ_live_regs.begin(), succ_live_regs.end());
        }
    }
    return live_registers;
}

// Iterative algorithm: implements a backward (liveness) analysis
static void findLiveRegisters(
    const std::list<CFGBlock> &blocks,
    FunEntry *function_entry,
    ASMSymbolTable *asm_symbol_table)
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
        std::set<GraphKey> old_annotations = s_blockAnnotations[block];
        std::set<GraphKey> end_live = meet(block, function_entry);
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
    ASMSymbolTable *asm_symbol_table)
{
    for (auto &block : blocks) {
        if (block.id == 0 || block.id == s_exitId)
            continue;
        for (auto &instr : block.instructions) {
            auto [used, updated] = findUsedAndUpdated(instr, asm_symbol_table);
            std::optional<GraphKey> mov_src;
            if (const Mov *mov = std::get_if<Mov>(&instr))
                mov_src = operandToKey(mov->src);
            const std::set<GraphKey> &live_keys = s_instructionAnnotations[&instr];
            for (const GraphKey &live_key : live_keys) {
                if (mov_src && *mov_src == live_key)
                    continue;
                for (auto &u : updated) {
                    std::optional<GraphKey> updated_key = operandToKey(u);
                    if (!updated_key)
                        continue;
                    if (interference_graph.contains(live_key) && interference_graph.contains(*updated_key)) {
                        if (live_key != *updated_key) {

/*
                            std::visit([&](auto&& k1, auto&& k2) {
                                auto printKey = [](const auto& k) -> std::string {
                                    using T = std::decay_t<decltype(k)>;
                                    if constexpr (std::is_same_v<T, std::string>) {
                                        return k;
                                    } else if constexpr (std::is_same_v<T, Register>) {
                                        return getEightByteName(k);
                                    }
                                };

                                std::cout << "Interference: "
                                          << printKey(k1) << " <-> " << printKey(k2)
                                          << std::endl;
                            }, live_key, *updated_key);
*/

                            interference_graph[live_key].neighbors.insert(*updated_key);
                            interference_graph[*updated_key].neighbors.insert(live_key);
                        }
                    } else {
                        // std::cout << "Something is live in the instruction, but it's not in the graph." << std::endl;
                    }
                }
            }
        }
    }
}

static inline long countUnprunedNeighbours(
    std::map<GraphKey, GraphData> &graph,
    const GraphData &node)
{
    auto &n = node.neighbors;
    return std::count_if(n.begin(), n.end(), [&](const GraphKey &k) {
        return !graph[k].pruned;
    });
}

static inline bool isCalleeSavedRegister(const GraphKey &key)
{
    if (const Register *reg = std::get_if<Register>(&key))
        return s_allCalleeSavedRegisters.contains(*reg);
    return false;
}

static void colorGraph(std::map<GraphKey, GraphData> &graph, uint8_t k)
{
    auto remaining = graph
        | std::views::filter([](const auto &p) {
            return !p.second.pruned;
        });
    if (remaining.empty())
        return;

    // Choose the next node to be pruned
    GraphKey chosen_key;
    GraphData *chosen_node = nullptr;

    for (auto &[key, data] : remaining) {
        long degree = countUnprunedNeighbours(graph, data);
        if (degree < k) {
            chosen_key = key;
            chosen_node = &data;
            break;
        }
    }

    if (!chosen_node) {
        double best_spill_metric = std::numeric_limits<double>::max();
        for (auto &[key, data] : remaining) {
            long degree = countUnprunedNeighbours(graph, data);
            double spill_metric = data.spill_cost / static_cast<double>(degree);
            if (spill_metric < best_spill_metric) {
                chosen_key = key;
                chosen_node = &data;
                best_spill_metric = spill_metric;
            }
        }
    }

    assert(chosen_node);
    chosen_node->pruned = true;

    // Color the rest of the graph
    colorGraph(graph, k);

    // Color this node
    auto v = std::views::iota(1, k + 1);
    std::set<size_t> colors(v.begin(), v.end());
    for (auto &neighbor_id : chosen_node->neighbors) {
        const GraphData &neighbor = graph.at(neighbor_id);
        if (neighbor.color != 0)
            colors.erase(neighbor.color);
    }
    if (colors.empty())
        return;

    // std::set is an ordered container, begin and rbegin work here
    if (isCalleeSavedRegister(chosen_key))
        chosen_node->color = *colors.rbegin(); // max
    else
        chosen_node->color = *colors.begin(); // min
    chosen_node->pruned = false;
}

static std::map<GraphKey, GraphData> buildInterferenceGraph(
    std::list<CFGBlock> &blocks,
    FunEntry *function_entry,
    ASMSymbolTable *asm_symbol_table,
    const std::vector<Register> &registers)
{
    bool processing_floating_points = registers[0] >= XMM0;

    // A fully connected base graph of the physical registers
    std::map<GraphKey, GraphData> interference_graph = buildBaseGraph(registers);
    // Extend it with pseudo-registers
    addPseudoRegisters(
        blocks,
        interference_graph,
        processing_floating_points,
        function_entry->aliased_vars,
        asm_symbol_table);
    // Finish the control flow graph before performing liveness analysis
    addControlFlowEdges(blocks);

    // Liveness analysis
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    s_exitId = blocks.back().id;
    findLiveRegisters(blocks, function_entry, asm_symbol_table);

    // We add edges to the interference graph using the liveness information
    addInterferenceEdges(blocks, interference_graph, asm_symbol_table);

    // Color the graph
    colorGraph(interference_graph, static_cast<uint8_t>(registers.size()));

    return interference_graph;
}

void addToRegisterMap(
    std::map<GraphKey, GraphData> &graph,
    std::map<std::string, Register> &register_map,
    FunEntry *function_entry)
{
    std::map<size_t, Register> color_map;
    for (auto &[key, data] : graph) {
        if (const Register *reg = std::get_if<Register>(&key))
            color_map[data.color] = *reg;
    }

    for (auto &[key, data] : graph) {
        if (const std::string *name = std::get_if<std::string>(&key)) {
            if (data.color == 0)
                continue;
            Register reg = color_map[data.color];
            register_map[*name] = reg;
            if (s_allCalleeSavedRegisters.contains(reg))
                function_entry->callee_saved_registers.insert(reg);
        }
    }
}

void allocateRegisters(
    std::list<CFGBlock> &blocks,
    FunEntry *function_entry,
    ASMSymbolTable *asm_symbol_table)
{
    // Mapping pseudo registers to physical registers
    std::map<std::string, Register> register_map;

    std::cout << std::endl << "Aliased vars for function:" << std::endl;
    for (auto &var_name : function_entry->aliased_vars)
        std::cout << var_name << std::endl;
    std::cout << std::endl;

    std::map<GraphKey, GraphData> int_graph
        = buildInterferenceGraph(blocks, function_entry, asm_symbol_table, s_integerRegisters);
    addToRegisterMap(int_graph, register_map, function_entry);

    std::map<GraphKey, GraphData> xmm_graph
        = buildInterferenceGraph(blocks, function_entry, asm_symbol_table, s_floatingPointRegisters);
    addToRegisterMap(xmm_graph, register_map, function_entry);

    std::cout << std::endl << "Register map for function:" << std::endl;
    for (auto &[name, reg] : register_map)
        std::cout << name << " -> " << getEightByteName(reg) << std::endl;
    std::cout << std::endl;

    replacePseudoRegisters(blocks, register_map, function_entry->callee_saved_registers);
}

} // assembly
