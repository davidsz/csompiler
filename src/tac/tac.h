#pragma once

#include "parser/ast_nodes.h"
#include "tac_nodes.h"

class Context;

namespace tac {

std::vector<TopLevel> from_ast(
    const std::vector<parser::Declaration> &list,
    Context *context);

} // namespace tac
