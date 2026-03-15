#include "tac.h"
#include "tac_builder.h"

namespace tac {

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
