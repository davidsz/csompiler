#include "asm_symbol_table.h"
#include <cassert>

namespace assembly {

ASMSymbolTable::ASMSymbolTable(
    std::shared_ptr<SymbolTable> symbolTable,
    std::shared_ptr<std::unordered_map<ConstantValue, std::string>> constants)
{
    for (const auto &[name, entry] : symbolTable->m_table) {
        if (entry.type.getAs<BasicType>()) {
            insert(name, ObjEntry{
                .type = entry.type.wordType(),
                .is_static = entry.attrs.type == IdentifierAttributes::Static
            });
        } else if (entry.type.getAs<FunctionType>()) {
            insert(name, FunEntry{
                .defined = entry.attrs.defined
            });
        } else
            assert(false);
    }

    for (auto const &[value, label] : *constants) {
        assert(std::holds_alternative<double>(value));
        insert(label, ObjEntry{
            .type = Doubleword,
            .is_static = true,
            .is_constant = true
        });
    }
}

void ASMSymbolTable::insert(const std::string &name, const ASMSymbolEntry &entry)
{
    m_table[name] = entry;
}

}; // assembly
