#include "optimization.h"

namespace tac {

static void constantFolding(std::vector<Instruction> &, bool &)
{
}

void apply_optimizations(std::vector<TopLevel> &list, const TACOptimizationArgs &arg)
{
    // Intraprocedural optimization: we iterate through the instructions of each
    // function body and process them separately.
    for (auto &top_level_obj : list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, FunctionDefinition>) {
                // We don't care about the phase ordering problem of optimizations,
                // we simply run them until they can't change the program anymore.
                bool changed = false;
                while (true) {
                    if (arg.constant_folding)
                        constantFolding(obj.inst, changed);
                    if (!changed)
                        break;
                }
            }
        }, top_level_obj);
    }
}

} // namespace tac
