#pragma once

#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

struct TypeTable {
    struct StructMemberEntry {
        std::string name;
        Type type;
        int offset;
    };
    struct StructEntry {
        int size;
        int alignment;
        std::vector<StructMemberEntry> members;
    };

    std::unordered_map<std::string, StructEntry> m_map;
};
