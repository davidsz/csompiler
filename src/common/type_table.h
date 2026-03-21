#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

enum AggregateClass {
    INTEGER,
    SSE,
    MEMORY
};

class TypeTable {
public:
    struct AggregateMemberEntry {
        std::string name;
        Type type;
        size_t offset;
    };
    struct AggregateEntry {
        std::vector<AggregateMemberEntry> members;
        std::vector<AggregateClass> eight_byte_classes = {};
        size_t size;
        size_t alignment;
        bool is_union = false;

        const AggregateMemberEntry *find(const std::string &name) const;
        const std::vector<AggregateClass> &classes(TypeTable *type_table);
    };

    void insert(const std::string &tag, const AggregateEntry &entry);
    AggregateEntry *get(const std::string &tag);
    bool contains(const std::string &tag) const;
    void print() const;

private:
    std::unordered_map<std::string, AggregateEntry> m_map;
};
