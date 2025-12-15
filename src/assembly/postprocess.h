#pragma once

#include "asm_nodes.h"
#include "asm_symbol_table.h"
#include "common/types.h"
#include <list>

namespace assembly {

void postprocessPseudoRegisters(
    std::list<TopLevel> &asmList,
    std::shared_ptr<ASMSymbolTable> asmSymbolTable);
void postprocessInvalidInstructions(std::list<TopLevel> &asmList);

}; // namespace assembly
