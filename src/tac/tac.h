#pragma once

#include "common/symbol_table.h"
#include "parser/ast_nodes.h"
#include "tac_nodes.h"

namespace tac {

std::vector<TopLevel> from_ast(
    const std::vector<parser::Declaration> &list,
    std::shared_ptr<SymbolTable> symbolTable);

} // namespace tac
