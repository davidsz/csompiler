#include "optimization.h"

namespace tac {

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
                    if (arg.constant_folding) {
                        constantFolding(obj.blocks, context, changed);
                    }
                } while (changed);
            }
        }, top_level_obj);
    }
}

} // namespace tac
