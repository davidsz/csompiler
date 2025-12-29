#pragma once

#include "asm_nodes.h"
#include "constant_map.h"
#include "common/symbol_table.h"

namespace assembly {

struct ObjEntry {
    WordType type;
    bool is_static;
    bool is_constant;
};

struct FunEntry {
    bool defined;
};

using ASMSymbolEntry = std::variant<ObjEntry, FunEntry>;

class ASMSymbolTable {
public:
    ASMSymbolTable(
        std::shared_ptr<SymbolTable> symbolTable,
        std::shared_ptr<ConstantMap> constants);
    template <typename T> T *getAs(const std::string &name)
    {
        if (m_table.contains(name))
            return std::get_if<T>(&m_table[name]);
        return nullptr;
    }

private:
    void insert(const std::string &name, const ASMSymbolEntry &entry);

    std::unordered_map<std::string, ASMSymbolEntry> m_table;
};

};
