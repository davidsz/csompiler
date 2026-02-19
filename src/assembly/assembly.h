#pragma once

#include "tac/tac_nodes.h"
#include "common/types.h"

class Context;

namespace assembly {

std::string from_tac(
    std::vector<tac::TopLevel>,
    Context *context);

}; // assembly
