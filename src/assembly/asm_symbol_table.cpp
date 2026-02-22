#include "asm_symbol_table.h"
#include "common/context.h"
#include <cassert>

namespace assembly {

ASMSymbolTable::ASMSymbolTable(
    Context *context,
    std::shared_ptr<ConstantMap> constants)
{
    TypeTable *type_table = context->typeTable.get();
    SymbolTable *symbol_table = context->symbolTable.get();

    for (const auto &[name, entry] : symbol_table->m_table) {
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
                    ByteArray{ entry.type.size(type_table), entry.type.alignment(type_table) }
                },
                .is_static = entry.attrs.type == IdentifierAttributes::Static
                    || entry.attrs.type == IdentifierAttributes::Constant,
                .is_constant = entry.attrs.type == IdentifierAttributes::Constant
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
