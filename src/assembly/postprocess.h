#pragma once

#include "asm_nodes.h"
#include "common/types.h"
#include <list>

namespace assembly {

void postprocessPseudoRegisters(
    std::list<TopLevel> &asmList,
    std::shared_ptr<SymbolTable> symbolTable);
void postprocessInvalidInstructions(std::list<TopLevel> &asmList);

}; // namespace assembly
