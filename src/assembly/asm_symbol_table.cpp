#include "asm_symbol_table.h"
#include <cassert>

namespace assembly {

void ASMSymbolTable::insert(const std::string &name, const ASMSymbolEntry &entry)
{
    m_table[name] = entry;
}

std::shared_ptr<ASMSymbolTable> CreateASMSymbolTable(std::shared_ptr<SymbolTable> symbolTable)
{
    std::shared_ptr<ASMSymbolTable> asmSymbolTable = std::make_shared<ASMSymbolTable>();
    for (const auto &[name, entry] : symbolTable->m_table) {
        if (int byte_size = entry.type.size()) {
            asmSymbolTable->insert(name, ObjEntry{
                .type = byte_size == 4 ? Longword : Quadword,
                .is_static = entry.attrs.type == IdentifierAttributes::Static
            });
        } else if (entry.type.getAs<FunctionType>()) {
            asmSymbolTable->insert(name, FunEntry{
                .defined = entry.attrs.defined
            });
        } else
            assert(false);
    }
    return asmSymbolTable;
}

}; // assembly
