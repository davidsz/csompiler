#pragma once

#include "tac/tac_nodes.h"
#include "common/types.h"

namespace assembly {

std::string from_tac(
    std::vector<tac::TopLevel>,
    std::shared_ptr<SymbolTable>);

}; // assembly
