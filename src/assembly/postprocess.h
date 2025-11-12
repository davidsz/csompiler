#pragma once

#include "asm_nodes.h"
#include <list>

namespace assembly {

int postprocessStackVariables(std::list<Instruction> &asmList);
void postprocessInvalidInstructions(std::list<Instruction> &asmList);

}; // namespace assembly
