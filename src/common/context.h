#pragma once

#include "type_table.h"
#include "symbol_table.h"

class Context {
public:
    std::shared_ptr<TypeTable> typeTable = std::make_shared<TypeTable>();
    std::shared_ptr<SymbolTable> symbolTable = std::make_shared<SymbolTable>(typeTable.get());
};
