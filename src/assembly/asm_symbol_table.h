#pragma once

#include "asm_nodes.h"
#include "common/symbol_table.h"

namespace assembly {

struct ObjEntry {
    WordType type;
    bool is_static;
};

struct FunEntry {
    bool defined;
};

using ASMSymbolEntry = std::variant<ObjEntry, FunEntry>;

struct ASMSymbolTable {
    template <typename T> T *getAs(const std::string &name)
    {
        if (m_table.contains(name))
            return std::get_if<T>(&m_table[name]);
        return nullptr;
    }
    void insert(const std::string &name, const ASMSymbolEntry &entry);

private:
    std::unordered_map<std::string, ASMSymbolEntry> m_table;
};

std::shared_ptr<ASMSymbolTable> CreateASMSymbolTable(std::shared_ptr<SymbolTable> symbolTable);

};
