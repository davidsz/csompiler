#pragma once

#include "types.h"
#include "values.h"
#include <unordered_map>

// Initial value types
struct Tentative {};
struct NoInitializer {};
struct Initial {
    ConstantValue i;
    auto operator<=>(const Initial &) const = default;
};
using InitialValue = std::variant<Tentative, NoInitializer, Initial>;

// Attributes of the symbols
struct IdentifierAttributes {
    enum AttrType {
        Function,
        Static,
        Local
    };
    AttrType type = Local;
    bool defined = false;
    bool global = false;
    InitialValue init = NoInitializer{};
};

struct SymbolEntry {
    Type type;
    IdentifierAttributes attrs;
};

struct SymbolTable {
public:
    bool contains(const std::string &name);
    const SymbolEntry *get(const std::string &name);
    template <typename T> const T *getTypeAs(const std::string &name) {
        auto it = m_table.find(name);
        if (it != m_table.end()) {
            return it->second.type.getAs<T>();
        }
        return nullptr;
    }
    int getByteSize(const std::string &name);
    WordType getWordType(const std::string &name);
    void insert(
        const std::string &name,
        const Type &type,
        const IdentifierAttributes &attr);
    void print();

    std::unordered_map<std::string, SymbolEntry> m_table;
};
