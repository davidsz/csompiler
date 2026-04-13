#pragma once

#include "asm_symbol_table.h"
#include "tac/tac_nodes.h"
#include "common/types.h"
#include <list>

class Context;

namespace assembly {

std::string from_tac(
    const std::list<tac::TopLevel> &tac_list,
    Context *context);

void postprocessPseudoRegisters(
    std::list<TopLevel> &asm_list,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table);

void postprocessInvalidInstructions(std::list<TopLevel> &asm_list);

}; // assembly
