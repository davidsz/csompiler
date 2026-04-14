#pragma once

#include "asm_nodes.h"

namespace assembly {

// Registers and PseudoRegisters
using GraphKey = std::variant<Register, std::string>;

struct GraphData {
    std::set<GraphKey> neighbors;
    double spill_cos = 0;
    size_t color = 0;
    bool pruned = false;
};

std::map<GraphKey, GraphData> buildInterferenceGraph(std::list<CFGBlock> &blocks);


} // assembly
