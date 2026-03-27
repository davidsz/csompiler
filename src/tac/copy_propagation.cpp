#include "tac_nodes.h"
#include "common/context.h"
#include <algorithm>
#include <map>

namespace tac {
/*
static void printValue(const Value &v)
{
    std::visit([](const auto &val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, Constant>) {
            std::visit([](const auto &c) {
                std::cout << toString(c);
            }, val.value);
        }
        else if constexpr (std::is_same_v<T, Variant>) {
            std::cout << val.name;
        }
    }, v);
}

static void printCopy(const Copy &c)
{
    std::cout << "[";
    printValue(c.dst);
    std::cout << " <- ";
    printValue(c.src);
    std::cout << "]";
}

static void debugReachingCopies(const std::set<Copy> &reaching_copies)
{
    std::cout << "Reaching copies (" << reaching_copies.size() << "):\n";

    for (const Copy &c : reaching_copies) {
        std::cout << "  ";
        printCopy(c);
        std::cout << "\n";
    }
}
*/

static std::map<const Instruction *, std::set<Copy>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<Copy>> s_blockAnnotations;

static inline bool isStatic(const Value &v, SymbolTable *symbol_table)
{
    if (std::holds_alternative<Constant>(v))
        return false;
    const Variant *var = std::get_if<Variant>(&v);
    assert(var);
    const SymbolEntry *entry = symbol_table->get(var->name);
    assert(entry);
    return entry->attrs.type == IdentifierAttributes::Static;
}

// Transfer function: takes all the Copy instructions that reach the begin-
// ning of a basic block and calculates which copies reach each individual
// instruction within the block. It also calculates which copies reach the
// end of the block, just after the final instruction.
static void transfer(
    const CFGBlock *block,
    const std::set<Copy> &initial_reaching_copies,
    SymbolTable *symbol_table)
{
    std::set<Copy> current_reaching_copies = initial_reaching_copies;
    for (const auto &instruction : block->instructions) {
        s_instructionAnnotations[&instruction] = current_reaching_copies;
        if (const Copy *copy = std::get_if<Copy>(&instruction)) {
            // Check for reversed copy
            if (current_reaching_copies.contains(Copy{ copy->dst, copy->src }))
                continue;
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (reaching_copy->src == copy->dst || reaching_copy->dst == copy->dst)
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
            current_reaching_copies.insert(*copy);
        }
        if (const FunctionCall *func_call = std::get_if<FunctionCall>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (isStatic(reaching_copy->src, symbol_table) || isStatic(reaching_copy->dst, symbol_table)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                if (func_call->dst && (reaching_copy->src == func_call->dst || reaching_copy->dst == func_call->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                ++reaching_copy;
            }
        }
        if (const Unary *unary = std::get_if<Unary>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (reaching_copy->src == unary->dst || reaching_copy->dst == unary->dst)
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                else
                    ++reaching_copy;
            }
        }
        if (const Binary *binary = std::get_if<Binary>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (reaching_copy->src == binary->dst || reaching_copy->dst == binary->dst)
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
static std::set<Copy> meet(
    const CFGBlock *block,
    const std::set<Copy> &all_copies)
{
    std::set<Copy> incoming_copies = all_copies;
    const std::set<CFGBlock *> &preds = block->predecessors;
    for (auto pred = preds.begin(); pred != preds.end(); ++pred) {
        if ((*pred)->id == 0) // Entry
            return {};
        const std::set<Copy> &pred_out_copies = s_blockAnnotations[*pred];
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
static void findReachingCopies(const std::list<CFGBlock> &blocks, SymbolTable *symbol_table)
{
    size_t exit_id = blocks.back().id;

    std::set<Copy> all_copies;
    for (auto &block : blocks) {
        for (auto &instr : block.instructions) {
            if (const Copy *copy = std::get_if<Copy>(&instr))
                all_copies.insert(*copy);
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
        std::set<Copy> old_annotations = s_blockAnnotations[block];
        std::set<Copy> incoming_copies = meet(block, all_copies);
        transfer(block, incoming_copies, symbol_table);
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
    std::set<Copy> &reaching_copies,
    bool &changed)
{
    if (std::holds_alternative<Constant>(operand))
        return operand;
    for (const Copy &rcopy : reaching_copies) {
        if (rcopy.dst == operand) {
            changed = true;
            return rcopy.src;
        }
    }
    return operand;
}

// Returns true if the instruction can be removed
static bool rewriteInstruction(
    Instruction &instr,
    std::set<Copy> &reaching_copies,
    bool &changed)
{
    if (Copy *copy = std::get_if<Copy>(&instr)) {
        for (const Copy &rcopy : reaching_copies) {
            if (rcopy == *copy || (rcopy.src == copy->dst && rcopy.dst == copy->src))
                return true;
        }
        copy->src = newOperand(copy->src, reaching_copies, changed);
    } else if (Unary *unary = std::get_if<Unary>(&instr))
        unary->src = newOperand(unary->src, reaching_copies, changed);
    else if (Binary *binary = std::get_if<Binary>(&instr)) {
        binary->src1 = newOperand(binary->src1, reaching_copies, changed);
        binary->src2 = newOperand(binary->src2, reaching_copies, changed);
    } else if (Return *ret = std::get_if<Return>(&instr))
        ret->val = newOperand(*ret->val, reaching_copies, changed);
    else if (FunctionCall *fc = std::get_if<FunctionCall>(&instr)) {
        for (auto &arg : fc->args)
            arg = newOperand(arg, reaching_copies, changed);
    } else if (JumpIfZero *jz = std::get_if<JumpIfZero>(&instr))
        jz->condition = newOperand(jz->condition, reaching_copies, changed);
    else if (JumpIfNotZero *jnz = std::get_if<JumpIfNotZero>(&instr))
        jnz->condition = newOperand(jnz->condition, reaching_copies, changed);
    return false;
}

void copyPropagation(std::list<CFGBlock> &blocks, Context *context, bool &changed)
{
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    findReachingCopies(blocks, context->symbolTable.get());
    for (auto &block : blocks) {
        for (auto it = block.instructions.begin(); it != block.instructions.end();) {
            if (rewriteInstruction(*it, s_instructionAnnotations[&*it], changed)) {
                changed = true;
                it = block.instructions.erase(it);
            } else
                ++it;
        }
    }
}

} // namespace tac
