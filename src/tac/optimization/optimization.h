#pragma once

#include "tac/tac_nodes.h"

namespace tac {

struct TACOptimizationArgs {
    bool constant_folding : 1;
    bool copy_propagation : 1;
    bool unreachable_code_elimination : 1;
    bool dead_store_elimination : 1;
};

void apply_optimizations(std::list<TopLevel> &list, const TACOptimizationArgs &args);

void constantFolding(std::list<Instruction> &instructions, bool &changed);

} // namespace tac
