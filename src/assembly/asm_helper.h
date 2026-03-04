#pragma once

#include "common/type_table.h"

namespace assembly {

// Break a struct into eight byte parts and classify them
enum StructClass {
    INTEGER,
    SSE,
    MEMORY
};
// TODO: Classifying a struct each time when we have to determine
// if it returns in memory or not is maybe an overkill
std::vector<StructClass> classifyStruct(const TypeTable::StructEntry *entry);

}; // assembly
