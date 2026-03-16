#include "tac.h"
#include "tac_builder.h"

namespace tac {

static void connect(CFGBlock *from, CFGBlock *to)
{
    from->successors.insert(to);
    to->predecessors.insert(from);
}

static void rebuildControlFlowEdges(std::list<CFGBlock> &blocks)
{
    // Map labels to their blocks
    std::map<std::string, CFGBlock *> blockLabels;
    for (auto &block : blocks) {
        block.predecessors.clear();
        block.successors.clear();
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
        if (block == entry_block) {
            connect(block, next_block);
            continue;
        }
        Instruction &last = block->instructions.back();
        if (std::holds_alternative<Return>(last))
            connect(block, exit_block);
        else if (const Jump *j = std::get_if<Jump>(&last)) {
            if (CFGBlock *target = blockLabels[j->target])
                connect(block, target);
        } else if (const JumpIfZero *jz = std::get_if<JumpIfZero>(&last)) {
            if (CFGBlock *target = blockLabels[jz->target])
                connect(block, target);
            connect(block, next_block);
        } else if (const JumpIfNotZero *jnz = std::get_if<JumpIfNotZero>(&last)) {
            if (CFGBlock *target = blockLabels[jnz->target])
                connect(block, target);
            connect(block, next_block);
        } else
            connect(block, next_block);
    }
}

void from_ast(
    const std::vector<parser::Declaration> &ast_root,
    std::list<tac::TopLevel> &top_level_out,
    Context *context)
{
    tac::TACBuilder astToTac(context);
    astToTac.ConvertTopLevel(ast_root, top_level_out);
}

void apply_optimizations(
    std::list<TopLevel> &list,
    const TACOptimizationArgs &arg,
    Context *context)
{
    // Intraprocedural optimization: we work on separate functions.
    for (auto &top_level_obj : list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, FunctionDefinition>) {
                // We don't care about the phase ordering problem of optimizations,
                // we simply run them until they can't change the program anymore.
                bool changed = false;
                do {
                    changed = false;
                    if (arg.constant_folding)
                        constantFolding(obj.blocks, context, changed);
                    rebuildControlFlowEdges(obj.blocks);
                    if (arg.unreachable_code_elimination)
                        unreachableCodeElimination(obj.blocks, changed);
                    if (arg.copy_propagation)
                        copyPropagation(obj.blocks, changed);
                    if (arg.dead_store_elimination)
                        deadStoreElimination(obj.blocks, changed);
                } while (changed);
            }
        }, top_level_obj);
    }
}

} // namespace tac
