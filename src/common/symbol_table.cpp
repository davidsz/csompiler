#include "symbol_table.h"
#include <iostream>

bool SymbolTable::contains(const std::string &name)
{
    return m_table.contains(name);
}

const SymbolEntry *SymbolTable::get(const std::string &name)
{
    auto it = m_table.find(name);
    if (it != m_table.end())
        return &it->second;
    return nullptr;
}

void SymbolTable::insert(const std::string &name, const Type &type, const IdentifierAttributes &attr)
{
    m_table[name] = SymbolEntry{ type , attr };
}

void SymbolTable::print()
{
    for (const auto &[name, entry] : m_table) {
        std::cout << name << " ";
        std::cout << "[" << entry.type << "] ";
        std::cout << (entry.attrs.defined ? "defined" : "undefined") << " ";
        std::cout << (entry.attrs.global ? "global" : "local") << " ";
        if (std::holds_alternative<Initial>(entry.attrs.init)) {
            auto &initial = std::get<Initial>(entry.attrs.init);
            std::cout << "Initial " << toString(initial.i);
        } else if (std::holds_alternative<Tentative>(entry.attrs.init))
            std::cout << "Tentative";
        else
            std::cout << "NoInitializer";
        std::cout << std::endl;
    }
}
