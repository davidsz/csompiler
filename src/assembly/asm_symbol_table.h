#pragma once

#include "asm_nodes.h"
#include "asm_helper.h"
#include "constant_map.h"
#include "common/symbol_table.h"

class Context;

namespace assembly {

struct ObjEntry {
    AssemblyType type;
    bool is_static;
    bool is_constant;
};

struct FunEntry {
    bool defined;
    bool return_on_stack;
    std::vector<Register> arg_registers = {};
    std::vector<Register> ret_registers = {};
    std::set<Register> callee_saved_registers = {};
};

using ASMSymbolEntry = std::variant<ObjEntry, FunEntry>;

class ASMSymbolTable {
public:
    void InsertSymbols(Context *context);
    void InsertConstants(std::shared_ptr<ConstantMap> constants);
    void Insert(const std::string &name, const ASMSymbolEntry &entry);
    template <typename T> T *getAs(const std::string &name)
    {
        if (m_table.contains(name))
            return std::get_if<T>(&m_table[name]);
        return nullptr;
    }

private:
    std::unordered_map<std::string, ASMSymbolEntry> m_table;
};

};
