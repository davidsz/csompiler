#include "asm_symbol_table.h"
#include "common/context.h"
#include <cassert>

namespace assembly {

void ASMSymbolTable::InsertSymbols(Context *context)
{
    TypeTable *type_table = context->typeTable.get();
    SymbolTable *symbol_table = context->symbolTable.get();

    for (const auto &[name, entry] : symbol_table->m_table) {
        if (entry.type.getAs<BasicType>()) {
            Insert(name, ObjEntry{
                .type = AssemblyType{ entry.type.wordType() },
                .is_static = entry.attrs.type == IdentifierAttributes::Static,
                .is_constant = false
            });
        } else if (entry.type.getAs<FunctionType>()) {
            // Defined functions are already added in the ASMBuilder
            // We only add placeholders here for the undefined ones
            if (!entry.attrs.defined) {
                Insert(name, FunEntry{
                    .defined = false,
                    .return_on_stack = false
                });
            }
        } else if (entry.type.getAs<PointerType>()) {
            Insert(name, ObjEntry{
                .type = AssemblyType{ Quadword },
                .is_static = entry.attrs.type == IdentifierAttributes::Static,
                .is_constant = false
            });
        } else if (entry.type.getAs<ArrayType>()) {
            Insert(name, ObjEntry{
                .type = AssemblyType{
                    ByteArray{ entry.type.size(type_table), entry.type.alignment(type_table) }
                },
                .is_static = entry.attrs.type == IdentifierAttributes::Static
                    || entry.attrs.type == IdentifierAttributes::Constant,
                .is_constant = entry.attrs.type == IdentifierAttributes::Constant
            });
        } else if (const AggregateType *aggr_type = entry.type.getAs<AggregateType>()) {
            AssemblyType type = AssemblyType{ ByteArray{ 0, 0} }; // Dummy type
            if (auto aggr_entry = type_table->get(aggr_type->tag))
                type = AssemblyType{ ByteArray{ aggr_entry->size, aggr_entry->alignment } };
            Insert(name, ObjEntry{
                .type = type,
                .is_static = entry.attrs.type == IdentifierAttributes::Static
                    || entry.attrs.type == IdentifierAttributes::Constant,
                .is_constant = entry.attrs.type == IdentifierAttributes::Constant
            });
        } else
            assert(false);
    }
}

void ASMSymbolTable::InsertConstants(std::shared_ptr<ConstantMap> constants)
{
    for (auto const &[value, label] : *constants) {
        assert(std::holds_alternative<double>(value));
        Insert(label, ObjEntry{
            .type = AssemblyType{ Doubleword },
            .is_static = true,
            .is_constant = true
        });
    }
}

void ASMSymbolTable::Insert(const std::string &name, const ASMSymbolEntry &entry)
{
    m_table[name] = entry;
}

}; // assembly
