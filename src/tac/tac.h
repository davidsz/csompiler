#pragma once

#include "parser/ast_nodes.h"
#include "tac_nodes.h"

class Context;

namespace tac {

void from_ast(
    const std::vector<parser::Declaration> &list,
    std::list<tac::TopLevel> &top_level_out,
    Context *context);

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

// constant_folding.cpp
void constantFolding(
    std::list<CFGBlock> &instructions,
    Context *context,
    bool &changed
);

// unreachable_code_elimination.cpp
void unreachableCodeElimination(
    std::list<CFGBlock> &blocks,
    bool &changed
);

// copy_propagation.cpp
void copyPropagation(
    std::list<CFGBlock> &blocks,
    const std::set<Value> &aliased_vars,
    Context *context,
    bool &changed
);

// dead_store_elimination.cpp
void deadStoreElimination(
    std::list<CFGBlock> &blocks,
    bool &changed
);

} // namespace tac
