#pragma once

#include "assembly/asm_symbol_table.h"
#include "type_table.h"
#include "symbol_table.h"

class Context {
public:
    std::shared_ptr<TypeTable> typeTable =
        std::make_shared<TypeTable>();
    std::shared_ptr<SymbolTable> symbolTable =
        std::make_shared<SymbolTable>(typeTable.get());
    std::shared_ptr<assembly::ASMSymbolTable> asmSymbolTable =
        std::make_shared<assembly::ASMSymbolTable>();

    bool constant_folding = false;
    bool copy_propagation = false;
    bool unreachable_code_elimination = false;
    bool dead_store_elimination = false;
};
