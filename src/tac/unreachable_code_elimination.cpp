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
    for (auto &pred : block->predecessors)
        pred->successors.remove(block);
    for (auto &succ : block->successors)
        succ->predecessors.remove(block);
    return blocks.erase(it);
}

void unreachableCodeElimination(std::list<CFGBlock> &blocks, bool &changed)
{
    // Removing unreachable blocks
    std::set<CFGBlock *> visited_blocks;
    visitBlocks(&blocks.front(), visited_blocks);
    for (auto it = blocks.begin(); it != blocks.end(); it++) {
        if (!visited_blocks.contains(&*it)) {
            it = removeBlock(blocks, it);
            changed = true;
        }
    }
}

} // namespace tac
