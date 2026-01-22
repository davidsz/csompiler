#include "asm_symbol_table.h"
#include <cassert>

namespace assembly {

ASMSymbolTable::ASMSymbolTable(
    std::shared_ptr<SymbolTable> symbolTable,
    std::shared_ptr<ConstantMap> constants)
{
    for (const auto &[name, entry] : symbolTable->m_table) {
        if (entry.type.getAs<BasicType>()) {
            insert(name, ObjEntry{
                .type = AssemblyType{ entry.type.wordType() },
                .is_static = entry.attrs.type == IdentifierAttributes::Static,
                .is_constant = false
            });
        } else if (entry.type.getAs<FunctionType>()) {
            insert(name, FunEntry{
                .defined = entry.attrs.defined
            });
        } else if (entry.type.getAs<PointerType>()) {
            insert(name, ObjEntry{
                .type = AssemblyType{ Quadword },
                .is_static = entry.attrs.type == IdentifierAttributes::Static,
                .is_constant = false
            });
        } else if (entry.type.getAs<ArrayType>()) {
            insert(name, ObjEntry{
                .type = AssemblyType{
                    ByteArray{ entry.type.size(), entry.type.alignment() }
                },
                .is_static = entry.attrs.type == IdentifierAttributes::Static,
                .is_constant = false
            });
        } else
            assert(false);
    }

    for (auto const &[value, label] : *constants) {
        assert(std::holds_alternative<double>(value));
        insert(label, ObjEntry{
            .type = AssemblyType{ Doubleword },
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
