#pragma once

#include "tac/tac_nodes.h"

class Context;

namespace tac {

struct TACOptimizationArgs {
    bool constant_folding : 1;
    bool copy_propagation : 1;
    bool unreachable_code_elimination : 1;
    bool dead_store_elimination : 1;
};

void apply_optimizations(
    std::list<TopLevel> &list,
    const TACOptimizationArgs &args,
    Context *context
);

void constantFolding(
    std::list<Instruction> &instructions,
    Context *context,
    bool &changed
);

} // namespace tac
