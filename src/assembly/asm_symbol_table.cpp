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
    for (const auto &[name, entry] : *symbolTable) {
        if (const BasicType *basic_type = entry.type.getAs<BasicType>()) {
            asmSymbolTable->insert(name, ObjEntry{
                .type = *basic_type == BasicType::Int ? Longword : Quadword,
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
