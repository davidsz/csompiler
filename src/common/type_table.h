#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

class TypeTable {
public:
    struct AggregateMemberEntry {
        std::string name;
        Type type;
        size_t offset;
    };
    struct AggregateEntry {
        size_t size;
        size_t alignment;
        std::vector<AggregateMemberEntry> members;

        const AggregateMemberEntry *find(const std::string &name) const;
    };

    void insert(const std::string &tag, const AggregateEntry &entry);
    const AggregateEntry *get(const std::string &tag) const;
    bool contains(const std::string &tag) const;
    void print() const;

private:
    std::unordered_map<std::string, AggregateEntry> m_map;
};
