#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

class TypeTable {
public:
    struct StructMemberEntry {
        std::string name;
        Type type;
        size_t offset;
    };
    struct StructEntry {
        size_t size;
        size_t alignment;
        std::vector<StructMemberEntry> members;

        const StructMemberEntry *find(const std::string &name) const;
    };

    void insert(const std::string &tag, const StructEntry &entry);
    const StructEntry *get(const std::string &tag) const;
    bool contains(const std::string &tag) const;

private:
    std::unordered_map<std::string, StructEntry> m_map;
};
