#include "tac_nodes.h"
#include "tac_helper.h"
#include <algorithm>
#include <map>

namespace tac {

static std::map<const Instruction *, std::set<Variant>> s_instructionAnnotations;
static std::map<const CFGBlock *, std::set<Variant>> s_blockAnnotations;
static size_t s_exitId = 0;

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
        ForEachValue(instruction, [&](const Value &v) {
            if (const Variant *var = std::get_if<Variant>(&v))
                current_live_variables.insert(*var);
        });
        if (std::holds_alternative<FunctionCall>(instruction)) {
            for (auto &v : aliased_vars) {
                if (const Variant *var = std::get_if<Variant>(&v))
                    current_live_variables.insert(*var);
            }
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
            const std::set<Variant> &succ_live_vars = s_blockAnnotations.at(succ);
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
