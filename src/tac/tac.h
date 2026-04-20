#pragma once

#include "parser/ast_nodes.h"
#include "tac_nodes.h"

class Context;

namespace tac {

void from_ast(
    const std::vector<parser::Declaration> &list,
    std::list<tac::TopLevel> &top_level_out,
    Context *context);

void apply_optimizations(
    std::list<TopLevel> &list,
    Context *context
);

} // namespace tac
