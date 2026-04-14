#include "interference_graph.h"
#include <map>

namespace assembly {

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

// Don't include SP and BP (they manage the stack frame);
// R10 and R11 are reserved for the instruction fixup phase
static const std::vector<Register> s_integerRegisters = {
    AX, BX, CX, DX, DI, SI, R8, R9, R12, R13, R14, R15
};

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

std::map<GraphKey, GraphData> buildInterferenceGraph(std::list<CFGBlock> &blocks)
{
    // A fully connected base graph of the physical registers
    std::map<GraphKey, GraphData> interference_graph = buildBaseGraph(s_integerRegisters);
    // Extend it with pseudo-registers
    addPseudoRegisters(blocks, interference_graph);
    // Finish the control flow graph before performing liveness analysis
    addControlFlowEdges(blocks);

    // TODO: Liveness analysis
    // TODO: Add edges to the interference graph

    return interference_graph;
}

} // assembly
