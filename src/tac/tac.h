#pragma once

#include "parser/ast_nodes.h"
#include "tac_nodes.h"

namespace tac {

std::vector<Instruction> from_ast(const std::vector<parser::Declaration> &list);

} // namespace tac
