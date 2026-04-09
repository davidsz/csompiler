#pragma once

#include "asm_nodes.h"
#include "asm_symbol_table.h"
#include "common/types.h"
#include <list>

namespace assembly {

void postprocessPseudoRegisters(
    std::list<TopLevel> &asm_list,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table);
void postprocessInvalidInstructions(std::list<TopLevel> &asm_list);

}; // namespace assembly
