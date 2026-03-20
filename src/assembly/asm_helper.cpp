#include "asm_helper.h"
#include "common/type_table.h"
#include <cassert>

namespace assembly {

static std::vector<Type> flattenStruct(const TypeTable::AggregateEntry *entry, const TypeTable *type_table)
{
    std::vector<Type> ret;
    for (auto &m : entry->members) {
        if (const AggregateType *aggr_type = m.type.getAs<AggregateType>()) {
            auto member_entry = type_table->get(aggr_type->tag);
            assert(member_entry);
            auto nested = flattenStruct(member_entry, type_table);
            ret.insert(ret.end(), nested.begin(), nested.end());
        } else if (const ArrayType *array_type = m.type.getAs<ArrayType>()) {
            for (size_t i = 0; i < array_type->count; i++) {
                if (const AggregateType *nested_aggr = array_type->element->getAs<AggregateType>()) {
                    auto member_entry = type_table->get(nested_aggr->tag);
                    assert(member_entry);
                    auto nested = flattenStruct(member_entry, type_table);
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

static std::vector<AggregateClass> classifyStruct(
    const TypeTable::AggregateEntry *entry,
    const TypeTable *type_table)
{
    std::vector<Type> flattened_types = flattenStruct(entry, type_table);
    bool first_is_double = flattened_types.front().storedType().isBasic(Double);
    bool last_is_double = flattened_types.back().storedType().isBasic(Double);
    if (entry->size > 8) {
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

static std::vector<AggregateClass> classifyUnion(
    const TypeTable::AggregateEntry *entry,
    const TypeTable *type_table)
{
    // Classify each member and merge by the dominance rule:
    // MEMORY > INTEGER > SSE
    std::vector<AggregateClass> result;
    if (entry->size > 8)
        result = { SSE, SSE }; // starting point, will be overridden
    else
        result = { SSE };

    for (auto &m : entry->members) {
        std::vector<AggregateClass> member_class;
        if (const AggregateType *aggr_type = m.type.getAs<AggregateType>()) {
            auto member_entry = type_table->get(aggr_type->tag);
            assert(member_entry);
            member_class = classifyAggregate(member_entry, type_table);
        } else if (const ArrayType *array_type = m.type.getAs<ArrayType>()) {
            AggregateClass elem_class = array_type->element->isBasic(Double) ? SSE : INTEGER;
            member_class = { elem_class };
            if (m.type.size(type_table) > 8)
                member_class.push_back(elem_class);
        } else {
            member_class = { m.type.storedType().isBasic(Double) ? SSE : INTEGER };
            if (m.type.size(type_table) > 8)
                member_class.push_back(INTEGER);
        }

        // Merge into result
        for (size_t i = 0; i < result.size(); i++) {
            if (i >= member_class.size())
                break;
            if (result[i] == MEMORY || member_class[i] == MEMORY)
                result[i] = MEMORY;
            else if (result[i] == INTEGER || member_class[i] == INTEGER)
                result[i] = INTEGER;
        }
    }
    return result;
}

std::vector<AggregateClass> classifyAggregate(const TypeTable::AggregateEntry *entry, const TypeTable *type_table)
{
    // Incomplete types shouldn't end up here
    if (entry->members.empty()) {
        assert(false);
        return {};
    }

    // The struct/union can't fit into two registers
    int size = static_cast<int>(entry->size);
    if (size > 16) {
        std::vector<AggregateClass> ret;
        while (size > 0) {
            ret.push_back(MEMORY);
            size -= 8;
        }
        return ret;
    }

    if (entry->is_union)
        return classifyUnion(entry, type_table);
    else
        return classifyStruct(entry, type_table);
}

}; // assembly
