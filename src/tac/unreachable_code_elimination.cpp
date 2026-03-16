#include "tac_nodes.h"

namespace tac {

static void visitBlocks(CFGBlock *current_block, std::set<CFGBlock *> &visited_blocks)
{
    if (visited_blocks.contains(current_block))
        return;
    visited_blocks.insert(current_block);
    for (auto &block : current_block->successors)
        visitBlocks(block, visited_blocks);
}

static std::list<CFGBlock>::iterator removeBlock(
    std::list<CFGBlock> &blocks,
    std::list<CFGBlock>::iterator it)
{
    CFGBlock *block = &*it;
    for (CFGBlock *pred : block->predecessors) {
        pred->successors.erase(block);
        for (CFGBlock *succ : block->successors) {
            if (std::find(pred->successors.begin(), pred->successors.end(), succ) == pred->successors.end()) {
                pred->successors.insert(succ);
                succ->predecessors.insert(pred);
            }
        }
    }
    for (CFGBlock *succ : block->successors)
        succ->predecessors.erase(block);
    return blocks.erase(it);
}

void unreachableCodeElimination(std::list<CFGBlock> &blocks, bool &changed)
{
    std::set<CFGBlock *> visited_blocks;
    visitBlocks(&blocks.front(), visited_blocks);
    for (auto it = blocks.begin(); it != blocks.end();) {
        // Removing unreachable blocks
        if (!visited_blocks.contains(&*it)) {
            it = removeBlock(blocks, it);
            changed = true;
            continue;
        } else
            ++it;
    }

    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
        if (it == blocks.begin() || it == std::prev(blocks.end()))
            continue;

        // Removing useless jumps
        Instruction &last_instruction = it->instructions.back();
        if (std::holds_alternative<Jump>(last_instruction)
            || std::holds_alternative<JumpIfZero>(last_instruction)
            || std::holds_alternative<JumpIfNotZero>(last_instruction)) {
            CFGBlock *default_successor = &*std::next(it);
            if (it->successors.size() == 1 && *it->successors.begin() == default_successor) {
                it->instructions.pop_back();
                changed = true;
            }
        }

        // Removing useless labels
        Instruction &first_instruction = it->instructions.front();
        if (std::holds_alternative<Label>(first_instruction)) {
            CFGBlock *default_predecessor = &*std::prev(it);
            if (it->predecessors.size() == 1 && *it->predecessors.begin() == default_predecessor) {
                it->instructions.pop_front();
                changed = true;
            }
        }
    }

    // Removing empty blocks
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (it == blocks.begin() || it == std::prev(blocks.end())) {
            ++it;
            continue;
        }
        if (it->instructions.empty()) {
            it = removeBlock(blocks, it);
            changed = true;
            continue;
        } else
            ++it;
    }
}

} // namespace tac
