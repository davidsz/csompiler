#include "tac.h"
#include "common/context.h"
#include "tac_builder.h"

namespace tac {

template <typename Fn>
static void forEachValue(const Instruction &instr, Fn &&fn)
{
    std::visit([&](const auto &i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Return>) {
            if (i.val)
                fn(*i.val);
        } else if constexpr (std::is_same_v<T, Unary>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Binary>) {
            fn(i.src1);
            fn(i.src2);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Copy>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, GetAddress>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Load>) {
            fn(i.src_ptr);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Store>) {
            fn(i.src);
            fn(i.dst_ptr);
        } else if constexpr (std::is_same_v<T, Jump>) {
        } else if constexpr (std::is_same_v<T, JumpIfZero>) {
            fn(i.condition);
        } else if constexpr (std::is_same_v<T, JumpIfNotZero>) {
            fn(i.condition);
        } else if constexpr (std::is_same_v<T, Label>) {
        } else if constexpr (std::is_same_v<T, FunctionCall>) {
            for (const auto &arg : i.args)
                fn(arg);
            if (i.dst)
                fn(*i.dst);
        } else if constexpr (std::is_same_v<T, SignExtend>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Truncate>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, ZeroExtend>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, DoubleToInt>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, DoubleToUInt>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, IntToDouble>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, UIntToDouble>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, AddPtr>) {
            fn(i.ptr);
            fn(i.index);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, CopyToOffset>) {
            fn(i.src);
        } else if constexpr (std::is_same_v<T, CopyFromOffset>) {
            fn(i.dst);
        }
    }, instr);
}

static std::set<Value> collectAliasedVariants(
    std::list<CFGBlock> &blocks,
    SymbolTable *symbol_table)
{
    std::set<Value> ret;
    for (auto &block : blocks) {
        for (const auto &instr : block.instructions) {
            // Address was taken by GetAddress
            if (const GetAddress *ga = std::get_if<GetAddress>(&instr))
                ret.insert(ga->src);
            // Include all static variables
            forEachValue(instr, [&](const Value &v) {
                if (const Variant *var = std::get_if<Variant>(&v)) {
                    const SymbolEntry *entry = symbol_table->get(var->name);
                    assert(entry);
                    if (entry->attrs.type == IdentifierAttributes::Static)
                        ret.insert(v);
                }
            });
        }
    }
    return ret;
}

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
                // std::cout << "--- Function " << obj.name << " ---\n";
                // We don't care about the phase ordering problem of optimizations,
                // we simply run them until they can't change the program anymore.
                bool changed = false;
                do {
                    changed = false;
                    std::set<Value> aliased_vars = collectAliasedVariants(
                        obj.blocks,
                        context->symbolTable.get());
                    if (arg.constant_folding)
                        constantFolding(obj.blocks, context, changed);
                    rebuildControlFlowEdges(obj.blocks);
                    if (arg.unreachable_code_elimination)
                        unreachableCodeElimination(obj.blocks, changed);
                    if (arg.copy_propagation)
                        copyPropagation(obj.blocks, aliased_vars, context, changed);
                    if (arg.dead_store_elimination)
                        deadStoreElimination(obj.blocks, changed);
                } while (changed);
            }
        }, top_level_obj);
    }
}

} // namespace tac
