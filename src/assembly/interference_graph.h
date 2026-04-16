#pragma once

#include "asm_nodes.h"
#include "asm_symbol_table.h"

namespace assembly {

// Registers and PseudoRegisters
using GraphKey = std::variant<Register, std::string>;

struct GraphData {
    std::set<GraphKey> neighbors = {};
    double spill_cost = 0;
    size_t color = 0;
    bool pruned = false;
};

std::map<GraphKey, GraphData> buildInterferenceGraph(
    std::list<CFGBlock> &blocks,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table);


} // assembly
