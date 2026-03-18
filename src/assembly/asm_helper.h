#pragma once

#include "common/type_table.h"

class TypeTable;

namespace assembly {

// Break the aggregate into eight byte parts and classify them
enum AggregateClass {
    INTEGER,
    SSE,
    MEMORY
};
// TODO: Classifying an aggregate each time when we have to determine
// if it returns in memory or not is maybe an overkill
// Implement some caching mechanism
std::vector<AggregateClass> classifyAggregate(const TypeTable::AggregateEntry *entry, const TypeTable *type_table);

}; // assembly
