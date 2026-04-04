#include "tac_nodes.h"
#include "tac_helper.h"
#include <algorithm>
#include <map>

namespace tac {

static std::map<const Instruction *, std::set<Variant>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<Variant>> s_blockAnnotations;
static size_t s_exitId = 0;

static inline void insertVariant(const Value &val, std::set<Variant> &variants)
{
    if (const Variant *var = std::get_if<Variant>(&val))
        variants.insert(*var);
}

static inline void removeVariant(const Value &val, std::set<Variant> &variants)
{
    if (const Variant *var = std::get_if<Variant>(&val))
        variants.erase(*var);
}

// Transfer function: takes the set of variables that are live at the end
// of a basic block and figures out which variables are live just before each
// instruction.
static void transfer(
    const CFGBlock *block,
    const std::set<Variant> &end_live_variables,
    const std::set<Value> &aliased_vars)
{
    std::set<Variant> current_live_variables = end_live_variables;
    for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
        const Instruction &instruction = *it;
        s_instructionAnnotations[&instruction] = current_live_variables;
        if (const Return *ret = std::get_if<Return>(&instruction)) {
            if (ret->val)
                insertVariant(*ret->val, current_live_variables);
        } else if (const Unary *unary = std::get_if<Unary>(&instruction)) {
            removeVariant(unary->dst, current_live_variables);
            insertVariant(unary->src, current_live_variables);
        } else if (const Binary *binary = std::get_if<Binary>(&instruction)) {
            removeVariant(binary->dst, current_live_variables);
            insertVariant(binary->src1, current_live_variables);
            insertVariant(binary->src2, current_live_variables);
        } else if (const Copy *copy = std::get_if<Copy>(&instruction)) {
            removeVariant(copy->dst, current_live_variables);
            insertVariant(copy->src, current_live_variables);
        } else if (const GetAddress *ga = std::get_if<GetAddress>(&instruction)) {
            removeVariant(ga->dst, current_live_variables);
            insertVariant(ga->src, current_live_variables);
        } else if (const Load *load = std::get_if<Load>(&instruction)) {
            removeVariant(load->dst, current_live_variables);
            insertVariant(load->src_ptr, current_live_variables);
        } else if (const Store *store = std::get_if<Store>(&instruction)) {
            // removeVariant(store->dst_ptr, current_live_variables);
            insertVariant(store->src, current_live_variables);
        } else if (/*const Jump *jump =*/ std::get_if<Jump>(&instruction)) {
        } else if (const JumpIfZero *jiz = std::get_if<JumpIfZero>(&instruction)) {
            insertVariant(jiz->condition, current_live_variables);
        } else if (const JumpIfNotZero *jinz = std::get_if<JumpIfNotZero>(&instruction)) {
            insertVariant(jinz->condition, current_live_variables);
        } else if (/*const Label *label =*/ std::get_if<Label>(&instruction)) {
        } else if (const FunctionCall *func_call = std::get_if<FunctionCall>(&instruction)) {
            if (func_call->dst)
                removeVariant(*func_call->dst, current_live_variables);
            for (const auto &arg : func_call->args)
                insertVariant(arg, current_live_variables);
            for (auto &v : aliased_vars)
                insertVariant(v, current_live_variables);
        } else if (const SignExtend *se = std::get_if<SignExtend>(&instruction)) {
            removeVariant(se->dst, current_live_variables);
            insertVariant(se->src, current_live_variables);
        } else if (const Truncate *tr = std::get_if<Truncate>(&instruction)) {
            removeVariant(tr->dst, current_live_variables);
            insertVariant(tr->src, current_live_variables);
        } else if (const ZeroExtend *ze = std::get_if<ZeroExtend>(&instruction)) {
            removeVariant(ze->dst, current_live_variables);
            insertVariant(ze->src, current_live_variables);
        } else if (const DoubleToInt *dti = std::get_if<DoubleToInt>(&instruction)) {
            removeVariant(dti->dst, current_live_variables);
            insertVariant(dti->src, current_live_variables);
        } else if (const DoubleToUInt *dtu = std::get_if<DoubleToUInt>(&instruction)) {
            removeVariant(dtu->dst, current_live_variables);
            insertVariant(dtu->src, current_live_variables);
        } else if (const IntToDouble *itd = std::get_if<IntToDouble>(&instruction)) {
            removeVariant(itd->dst, current_live_variables);
            insertVariant(itd->src, current_live_variables);
        } else if (const UIntToDouble *utd = std::get_if<UIntToDouble>(&instruction)) {
            removeVariant(utd->dst, current_live_variables);
            insertVariant(utd->src, current_live_variables);
        } else if (const AddPtr *add = std::get_if<AddPtr>(&instruction)) {
            removeVariant(add->dst, current_live_variables);
            insertVariant(add->ptr, current_live_variables);
            insertVariant(add->index, current_live_variables);
        } else if (const CopyToOffset *cto = std::get_if<CopyToOffset>(&instruction)) {
            removeVariant(Value{ Variant{ cto->dst_identifier } }, current_live_variables);
            insertVariant(cto->src, current_live_variables);
        } else if (const CopyFromOffset *cfo = std::get_if<CopyFromOffset>(&instruction)) {
            removeVariant(cfo->dst, current_live_variables);
            insertVariant(Value{ Variant{ cfo->src_identifier } }, current_live_variables);
        }
    }
    s_blockAnnotations[block] = std::move(current_live_variables);
}

// Meet operator: propagates information about live variables
// from one block to another.
static std::set<Variant> meet(
    const CFGBlock *block,
    const std::set<Value> &aliased_vars)
{
    std::set<Variant> live_variables;
    for (auto &succ : block->successors) {
        if (succ->id == 0)
            assert(false);
        else if (succ->id == s_exitId) {
            for (auto &v : aliased_vars) {
                if (const Variant *var = std::get_if<Variant>(&v))
                    live_variables.insert(*var);
            }
        } else {
            const std::set<Variant> &succ_live_vars = s_blockAnnotations[succ];
            live_variables.insert(succ_live_vars.begin(), succ_live_vars.end());
        }
    }
    return live_variables;
}

// Iterative algorithm: implements a backward (liveness) analysis
static void findLiveVariables(
    const std::list<CFGBlock> &blocks,
    const std::set<Value> &aliased_vars)
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
        std::set<Variant> old_annotations = s_blockAnnotations[block];
        std::set<Variant> end_live = meet(block, aliased_vars);
        transfer(block, end_live, aliased_vars);
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

static bool isDeadStore(const Instruction &instr)
{
    // Self-assignment is always a dead store
    if (const Copy *copy = std::get_if<Copy>(&instr)) {
        if (copy->src == copy->dst)
            return true;
    }

    const std::set<Variant> &live = s_instructionAnnotations[&instr];
    Variant dst;
    bool has_dst = std::visit([&dst](const auto &i) -> bool {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, FunctionCall> || std::is_same_v<T, Store>)
            return false;
        else if constexpr (requires { i.dst; }) {
            if (const Variant *var = std::get_if<Variant>(&i.dst)) {
                dst = *var;
                return true;
            }
        }
        return false;
    }, instr);

    if (!has_dst)
        return false;

    return !live.contains(dst);
}

void deadStoreElimination(
    std::list<CFGBlock> &blocks,
    const std::set<Value> &aliased_vars,
    bool &changed)
{
    s_instructionAnnotations.clear();
    s_blockAnnotations.clear();
    s_exitId = blocks.back().id;
    findLiveVariables(blocks, aliased_vars);
    for (auto block = blocks.begin(); block != blocks.end(); ++block) {
        for (auto i = block->instructions.begin(); i != block->instructions.end();) {
            if (isDeadStore(*i)) {
                changed = true;
                i = block->instructions.erase(i);
            } else
                ++i;
        }
    }
}

} // namespace tac
