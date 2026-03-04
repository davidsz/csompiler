#include "asm_helper.h"
#include <cassert>

namespace assembly {

std::vector<StructClass> classifyStruct(const TypeTable::StructEntry *entry)
{
    if (entry->members.empty()) {
        assert(false);
        return {};
    }

    // The struct can't fit on two registers
    size_t size = entry->size;
    if (size > 16) {
        std::vector<StructClass> ret;
        while (size > 0) {
            ret.push_back(MEMORY);
            size -= 8;
        }
        return ret;
    }

    // Classify the two registers
    bool first_is_double = entry->members.front().type.storedType().isBasic(Double);
    bool last_is_double = entry->members.back().type.storedType().isBasic(Double);
    if (size > 8) {
        if (first_is_double && last_is_double)
            return { SSE, SSE };
        if (first_is_double)
            return { SSE, INTEGER };
        if (last_is_double)
            return { INTEGER, SSE };
        return { INTEGER, INTEGER };
    } else if (first_is_double)
        return { SSE };
    else
        return { INTEGER };
}

}; // assembly
