#pragma once

#include "tac/tac_nodes.h"

class Context;

namespace assembly {

std::string from_tac(
    const std::list<tac::TopLevel> &tac_list,
    Context *context);

}; // assembly
