#include "asm_helper.h"
#include "common/type_table.h"
#include <cassert>

namespace assembly {

static std::vector<Type> flattenTypes(const TypeTable::AggregateEntry *entry, const TypeTable *type_table)
{
    std::vector<Type> ret;
    for (auto &m : entry->members) {
        if (const AggregateType *aggr_type = m.type.getAs<AggregateType>()) {
            auto member_entry = type_table->get(aggr_type->tag);
            assert(member_entry);
            auto nested = flattenTypes(member_entry, type_table);
            ret.insert(ret.end(), nested.begin(), nested.end());
        } else if (const ArrayType *array_type = m.type.getAs<ArrayType>()) {
            for (size_t i = 0; i < array_type->count; i++) {
                if (const AggregateType *nested_aggr = array_type->element->getAs<AggregateType>()) {
                    auto member_entry = type_table->get(nested_aggr->tag);
                    assert(member_entry);
                    auto nested = flattenTypes(member_entry, type_table);
                    ret.insert(ret.end(), nested.begin(), nested.end());
                } else {
                    ret.push_back(*array_type->element);
                }
            }
        } else {
            ret.push_back(m.type);
        }
    }
    return ret;
}

std::vector<AggregateClass> classifyAggregate(const TypeTable::AggregateEntry *entry, const TypeTable *type_table)
{
    if (entry->members.empty()) {
        assert(false);
        return {};
    }

    // The struct/union can't fit on two registers
    int size = static_cast<int>(entry->size);
    if (size > 16) {
        std::vector<AggregateClass> ret;
        while (size > 0) {
            ret.push_back(MEMORY);
            size -= 8;
        }
        return ret;
    }

    // Classify the two registers
    std::vector<Type> flattened_types = flattenTypes(entry, type_table);
    bool first_is_double = flattened_types.front().storedType().isBasic(Double);
    bool last_is_double = flattened_types.back().storedType().isBasic(Double);
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
