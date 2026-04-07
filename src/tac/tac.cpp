#include "tac.h"
#include "common/context.h"
#include "tac_builder.h"
#include "tac_helper.h"
#include "tac_printer.h"

namespace tac {

static std::set<Value> collectAliasedVariants(std::list<CFGBlock> &blocks)
{
    std::set<Value> ret;
    for (auto &block : blocks) {
        for (const auto &instr : block.instructions) {
            if (const GetAddress *ga = std::get_if<GetAddress>(&instr))
                ret.insert(ga->src);
        }
    }
    return ret;
}

static std::set<Value> collectStaticVariants(
    std::list<CFGBlock> &blocks,
    SymbolTable *symbol_table)
{
    std::set<Value> ret;
    for (auto &block : blocks) {
        for (const auto &instr : block.instructions) {
            ForEachValue(instr, [&](const Value &v) {
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
                std::cout << "--- Function \"" << obj.name << "\" ---\n";
                TACPrinter printer;
                // We don't care about the phase ordering problem of optimizations,
                // we simply run them until they can't change the program anymore.
                bool changed = false;
                do {
                    std::cout << "--- iteration begin ---\n";
                    changed = false;
                    std::set<Value> aliased_vars = collectAliasedVariants(obj.blocks);
                    std::set<Value> static_vars = collectStaticVariants(
                        obj.blocks,
                        context->symbolTable.get());
                    if (arg.constant_folding)
                        constantFolding(obj.blocks, context, changed);
                    rebuildControlFlowEdges(obj.blocks);
                    if (arg.unreachable_code_elimination)
                        unreachableCodeElimination(obj.blocks, changed);
                    if (arg.copy_propagation)
                        copyPropagation(obj.blocks, aliased_vars, static_vars, context, changed);
                    if (arg.dead_store_elimination)
                        deadStoreElimination(obj.blocks, aliased_vars, static_vars, changed);
                    std::cout << "--- function state after the iteration: ---\n";
                    printer.PrintFunction(obj);
                } while (changed);
            }
        }, top_level_obj);
    }
}

} // namespace tac
