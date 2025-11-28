#pragma once

#include "asm_nodes.h"
#include <list>

namespace assembly {

void postprocessStackVariables(std::list<TopLevel> &asmList);
void postprocessInvalidInstructions(std::list<TopLevel> &asmList);

}; // namespace assembly
