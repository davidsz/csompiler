#include "tac_nodes.h"
#include "common/context.h"
#include <algorithm>
#include <map>

namespace tac {

static std::map<const Instruction *, std::set<Copy>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<Copy>> s_blockAnnotations;

static Type getType(const Value &value, SymbolTable *symbol_table)
{
    if (auto c = std::get_if<Constant>(&value))
        return getType(c->value);
    auto v = std::get_if<Variant>(&value);
    const SymbolEntry *entry = symbol_table->get(v->name);
    assert(entry);
    return entry->type;
}

static bool isNullConstant(const Value &value)
{
    if (auto c = std::get_if<Constant>(&value)) {
        return std::visit([](const auto &v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_integral_v<T>)
                return v == 0;
            return false;
        }, c->value);
    }
    return false;
}

static void killCopiesUsing(std::set<Copy> &copies, const Value &v)
{
    for (auto it = copies.begin(); it != copies.end();) {
        if (it->src == v || it->dst == v)
            it = copies.erase(it);
        else
            ++it;
    }
}

// Transfer function: takes all the Copy instructions that reach the begin-
// ning of a basic block and calculates which copies reach each individual
// instruction within the block. It also calculates which copies reach the
// end of the block, just after the final instruction.
static void transfer(
    const CFGBlock *block,
    const std::set<Copy> &initial_reaching_copies,
    const std::set<Value> &aliased_vars,
    const std::set<Value> &static_vars,
    SymbolTable *symbol_table)
{
    std::set<Copy> current_reaching_copies = initial_reaching_copies;
    for (const auto &instruction : block->instructions) {
        s_instructionAnnotations[&instruction] = current_reaching_copies;
        if (const Copy *copy = std::get_if<Copy>(&instruction)) {
            // Check for reversed copy
            if (current_reaching_copies.contains(Copy{ copy->dst, copy->src }))
                continue;
            killCopiesUsing(current_reaching_copies, copy->dst);
            Type src_type = getType(copy->src, symbol_table);
            Type dst_type = getType(copy->dst, symbol_table);
            if (std::holds_alternative<Constant>(copy->src) /*&& src_type == dst_type*/) {
/*
                Copy converted = *copy;
                Constant &c = std::get<Constant>(converted.src);
                c.value = ConvertValue(c.value, dst_type);
                current_reaching_copies.insert(converted);
*/
                current_reaching_copies.insert(*copy);
            } else if (src_type == dst_type
                || src_type.isSigned() == dst_type.isSigned()) {
                current_reaching_copies.insert(*copy);
            } else if (isNullConstant(copy->src) && dst_type.isPointer())
                current_reaching_copies.insert(*copy);
        } else if (const FunctionCall *func_call = std::get_if<FunctionCall>(&instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (aliased_vars.contains(reaching_copy->src) || aliased_vars.contains(reaching_copy->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                if (static_vars.contains(reaching_copy->src) || static_vars.contains(reaching_copy->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                if (func_call->dst && (reaching_copy->src == func_call->dst || reaching_copy->dst == func_call->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                ++reaching_copy;
            }
        } else if (std::holds_alternative<Store>(instruction)) {
            for (auto reaching_copy = current_reaching_copies.begin();
                reaching_copy != current_reaching_copies.end();) {
                if (aliased_vars.contains(reaching_copy->src) || aliased_vars.contains(reaching_copy->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                if (static_vars.contains(reaching_copy->src) || static_vars.contains(reaching_copy->dst)) {
                    reaching_copy = current_reaching_copies.erase(reaching_copy);
                    continue;
                }
                ++reaching_copy;
            }
        } else if (const Unary *unary = std::get_if<Unary>(&instruction))
            killCopiesUsing(current_reaching_copies, unary->dst);
        else if (const Binary *binary = std::get_if<Binary>(&instruction))
            killCopiesUsing(current_reaching_copies, binary->dst);
        else if (const SignExtend *se = std::get_if<SignExtend>(&instruction))
            killCopiesUsing(current_reaching_copies, se->dst);
        else if (const Truncate *tr = std::get_if<Truncate>(&instruction))
            killCopiesUsing(current_reaching_copies, tr->dst);
        else if (const ZeroExtend *ze = std::get_if<ZeroExtend>(&instruction))
            killCopiesUsing(current_reaching_copies, ze->dst);
        else if (const DoubleToInt *dti = std::get_if<DoubleToInt>(&instruction))
            killCopiesUsing(current_reaching_copies, dti->dst);
        else if (const DoubleToUInt *dtu = std::get_if<DoubleToUInt>(&instruction))
            killCopiesUsing(current_reaching_copies, dtu->dst);
        else if (const IntToDouble *itd = std::get_if<IntToDouble>(&instruction))
            killCopiesUsing(current_reaching_copies, itd->dst);
        else if (const UIntToDouble *utd = std::get_if<UIntToDouble>(&instruction))
            killCopiesUsing(current_reaching_copies, utd->dst);
        else if (const AddPtr *add = std::get_if<AddPtr>(&instruction))
            killCopiesUsing(current_reaching_copies, add->dst);
        else if (const CopyToOffset *cto = std::get_if<CopyToOffset>(&instruction))
            killCopiesUsing(current_reaching_copies, Value{ Variant{ cto->dst_identifier } });
        else if (const CopyFromOffset *cfo = std::get_if<CopyFromOffset>(&instruction))
            killCopiesUsing(current_reaching_copies, cfo->dst);
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
static void findReachingCopies(
    const std::list<CFGBlock> &blocks,
    const std::set<Value> &aliased_vars,
    const std::set<Value> &static_vars,
    SymbolTable *symbol_table)
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
        transfer(block, incoming_copies, aliased_vars, static_vars, symbol_table);
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
    bool &changed)
{
    std::set<Copy> &reaching_copies = s_instructionAnnotations[&instr];
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
    else if (Load *load = std::get_if<Load>(&instr))
        load->src_ptr = newOperand(load->src_ptr, reaching_copies, changed);
    else if (Store *store = std::get_if<Store>(&instr))
        store->src = newOperand(store->src, reaching_copies, changed);
    else if (FunctionCall *fc = std::get_if<FunctionCall>(&instr)) {
        for (auto &arg : fc->args)
            arg = newOperand(arg, reaching_copies, changed);
    } else if (JumpIfZero *jz = std::get_if<JumpIfZero>(&instr))
        jz->condition = newOperand(jz->condition, reaching_copies, changed);
    else if (JumpIfNotZero *jnz = std::get_if<JumpIfNotZero>(&instr))
        jnz->condition = newOperand(jnz->condition, reaching_copies, changed);
    else if (SignExtend *se = std::get_if<SignExtend>(&instr))
        se->src = newOperand(se->src, reaching_copies, changed);
    else if (Truncate *tr = std::get_if<Truncate>(&instr))
        tr->src = newOperand(tr->src, reaching_copies, changed);
    else if (ZeroExtend *ze = std::get_if<ZeroExtend>(&instr))
        ze->src = newOperand(ze->src, reaching_copies, changed);
    else if (DoubleToInt *dti = std::get_if<DoubleToInt>(&instr))
        dti->src = newOperand(dti->src, reaching_copies, changed);
    else if (DoubleToUInt *dtu = std::get_if<DoubleToUInt>(&instr))
        dtu->src = newOperand(dtu->src, reaching_copies, changed);
    else if (IntToDouble *itd = std::get_if<IntToDouble>(&instr))
        itd->src = newOperand(itd->src, reaching_copies, changed);
    else if (UIntToDouble *utd = std::get_if<UIntToDouble>(&instr))
        utd->src = newOperand(utd->src, reaching_copies, changed);
    else if (AddPtr *add = std::get_if<AddPtr>(&instr)) {
        add->ptr = newOperand(add->ptr, reaching_copies, changed);
        add->index = newOperand(add->index, reaching_copies, changed);
    } else if (CopyToOffset *cto = std::get_if<CopyToOffset>(&instr))
        cto->src = newOperand(cto->src, reaching_copies, changed);
    else if (CopyFromOffset *cfo = std::get_if<CopyFromOffset>(&instr)) {
        Value old_src = Value{ Variant{ cfo->src_identifier } };
        Value new_src = newOperand(old_src, reaching_copies, changed);
        cfo->src_identifier = std::get_if<Variant>(&new_src)->name;
    }
    return false;
}

void copyPropagation(
    std::list<CFGBlock> &blocks,
    const std::set<Value> &aliased_vars,
    const std::set<Value> &static_vars,
    Context *context,
    bool &changed)
{
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    findReachingCopies(blocks, aliased_vars, static_vars, context->symbolTable.get());
    for (auto &block : blocks) {
        for (auto it = block.instructions.begin(); it != block.instructions.end();) {
            if (rewriteInstruction(*it, changed)) {
                changed = true;
                it = block.instructions.erase(it);
            } else
                ++it;
        }
    }
}

} // namespace tac
