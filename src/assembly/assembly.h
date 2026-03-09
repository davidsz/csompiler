#pragma once

#include "tac/tac_nodes.h"
#include "common/types.h"
#include <list>

class Context;

namespace assembly {

std::string from_tac(
    std::list<tac::TopLevel>,
    Context *context);

}; // assembly
