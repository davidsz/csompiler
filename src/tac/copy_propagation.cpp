#include "tac_nodes.h"
#include "common/context.h"
#include <map>

namespace tac {

static std::map<const Instruction *, std::set<const Copy *>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<const Copy *>> s_blockAnnotations;

static bool containsReverseCopy(
    const std::set<const Copy *> &copies,
    const Copy *copy)
{
    for (const Copy *rc : copies)
        if (rc->src == copy->dst && rc->dst == copy->src)
            return true;
    return false;
}

// Transfer function: takes all the Copy instructions that reach the begin-
// ning of a basic block and calculates which copies reach each individual
// instruction within the block. It also calculates which copies reach the
// end of the block, just after the final instruction.
static void transfer(
    const CFGBlock *block,
    const std::set<const Copy *> &initial_reaching_copies)
{
    std::set<const Copy *> current_reaching_copies = initial_reaching_copies;
    for (const auto &instruction : block->instructions) {
        s_instructionAnnotations[&instruction] = current_reaching_copies;
        if (const Copy *copy = std::get_if<Copy>(&instruction)) {
            if (containsReverseCopy(current_reaching_copies, copy))
                continue;
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if ((*reaching_copy)->src == copy->dst || (*reaching_copy)->dst == copy->dst)
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
            current_reaching_copies.insert(copy);
        }
        if (const FunctionCall *func_call = std::get_if<FunctionCall>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                // TODO: if copy.src is static or copy.dst is static -> remove
                /*
                const SymbolEntry *entry = m_context->symbolTable->get(v.identifier);
                assert(entry);
                if (entry->attrs.type == IdentifierAttributes::Static)
                */
                if (func_call->dst && ((*reaching_copy)->src == func_call->dst || (*reaching_copy)->dst == func_call->dst))
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
        }
        if (const Unary *unary = std::get_if<Unary>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if ((*reaching_copy)->src == unary->dst || (*reaching_copy)->dst == unary->dst)
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
        }
        if (const Binary *binary = std::get_if<Binary>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if ((*reaching_copy)->src == binary->dst || (*reaching_copy)->dst == binary->dst)
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
        }
    }
    s_blockAnnotations[block] = std::move(current_reaching_copies);
}


// Meet operator: propagates information about reaching copies
// from one block to another.
static std::set<const Copy *> meet(
    const CFGBlock *block,
    const std::set<const Copy *> &all_copies)
{
    std::set<const Copy *> incoming_copies = all_copies;
    const std::set<CFGBlock *> &preds = block->predecessors;
    for (auto pred = preds.begin(); pred != preds.end(); ++pred) {
        if ((*pred)->id == 0) // Entry
            return {};
        const std::set<const Copy *> &pred_out_copies = s_blockAnnotations[*pred];
        // Intersection of incoming_copies and pred_out_copies
        for (auto it = incoming_copies.begin(); it != incoming_copies.end();) {
            if (!pred_out_copies.contains(*it))
                it = incoming_copies.erase(it);
            else
                ++it;
        }
    }
    return incoming_copies;
}

// Iterative algorithm: implements a forward data flow analysis using
// the transfer function and meet operator defined above.
static void findReachingCopies(const std::list<CFGBlock> &blocks)
{
    size_t exit_id = blocks.back().id;

    std::set<const Copy *> all_copies;
    for (auto &block : blocks) {
        for (auto &instr : block.instructions) {
            if (const Copy *copy = std::get_if<Copy>(&instr))
                all_copies.insert(copy);
        }
    }

    std::list<const CFGBlock *> worklist;
    // TODO: Can be optimized by reverse postordering
    for (auto &block : blocks) {
        if (block.id == 0 || block.id == exit_id)
            continue;
        worklist.push_back(&block);
        s_blockAnnotations[&block] = all_copies;
    }

    while (!worklist.empty()) {
        const CFGBlock *block = worklist.front();
        worklist.pop_front();
        std::set<const Copy *> old_annotations = s_blockAnnotations[block];
        std::set<const Copy *> incoming_copies = meet(block, all_copies);
        transfer(block, incoming_copies);
        if (old_annotations != s_blockAnnotations[block]) {
            for (auto succ : block->successors) {
                if (succ->id == 0)
                    assert(false);
                if (succ->id == exit_id)
                    continue;
                if (std::find(worklist.begin(), worklist.end(), succ) == worklist.end())
                    worklist.push_back(succ);
            }
        }
    }
}

static Value newOperand(
    Value &operand,
    std::set<const Copy *> &reaching_copies,
    bool &changed)
{
    if (std::holds_alternative<Constant>(operand))
        return operand;
    for (auto rcopy : reaching_copies) {
        if (rcopy->dst == operand) {
            changed = true;
            return rcopy->src;
        }
    }
    return operand;
}

static void rewriteInstruction(
    Instruction &instr,
    std::set<const Copy *> &reaching_copies,
    bool &changed)
{
    if (Copy *copy = std::get_if<Copy>(&instr)) {
        for (auto rcopy : reaching_copies) {
            if (rcopy == copy || (rcopy->src == copy->dst && rcopy->dst == copy->src))
                return;
        }
        copy->src = newOperand(copy->src, reaching_copies, changed);
        return;
    }
    if (Unary *unary = std::get_if<Unary>(&instr)) {
        unary->src = newOperand(unary->src, reaching_copies, changed);
        return;
    }
    if (Binary *binary = std::get_if<Binary>(&instr)) {
        binary->src1 = newOperand(binary->src1, reaching_copies, changed);
        binary->src2 = newOperand(binary->src2, reaching_copies, changed);
        return;
    }
    if (Return *ret = std::get_if<Return>(&instr)) {
        ret->val = newOperand(*ret->val, reaching_copies, changed);
        return;
    }
}

void copyPropagation(std::list<CFGBlock> &blocks, Context *, bool &changed)
{
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    findReachingCopies(blocks);
    for (auto &block : blocks) {
        for (auto &instr : block.instructions)
            rewriteInstruction(instr, s_instructionAnnotations[&instr], changed);
    }
}

} // namespace tac
