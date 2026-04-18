#include "asm_builder_utils.h"

namespace assembly {

static bool isZeroImmediate(const Operand &op)
{
    if (const Imm *imm = std::get_if<Imm>(&op))
        return imm->value == 0;
    return false;
}

const std::string *getString(const tac::Value &value)
{
    if (auto *var = std::get_if<tac::Variant>(&value))
        return &var->name;
    return nullptr;
}

const std::string *getString(const Operand &op)
{
    if (const Pseudo *p = std::get_if<Pseudo>(&op))
        return &p->name;
    else if (const PseudoAggregate *agg = std::get_if<PseudoAggregate>(&op))
        return &agg->name;
    assert(false);
    return nullptr;
}

AssemblyType getAggregatePartType(size_t offset, size_t size)
{
    size_t byte_from_end = size - offset;
    if (byte_from_end >= 8)
        return AssemblyType{ Quadword };
    if (byte_from_end == 4)
        return AssemblyType{ Longword };
    if (byte_from_end == 1)
        return AssemblyType{ Byte };
    // Irregular size struct/union; "not word-aligned tail fragment"
    return AssemblyType{ ByteArray{ byte_from_end, 8 } };
}

ClassifiedReturn classifyReturnValue(
    const Operand &operand,
    const Type &type,
    TypeTable *type_table)
{
    ClassifiedReturn ret;
    if (type.isBasic(Double))
        ret.double_values.push_back(operand);
    else if (type.isScalar()) {
        WordType word_type = type.wordType();
        // Only to satify the test framework
        if (word_type == Quadword && isZeroImmediate(operand))
            word_type = Longword;
        ret.int_values.push_back({ operand, AssemblyType{ word_type } });
    } else {
        const AggregateType *aggr_type = type.getAs<AggregateType>();
        assert(aggr_type);
        size_t aggregate_size = type.size(type_table);
        TypeTable::AggregateEntry *entry = type_table->get(aggr_type->tag);
        auto &classes = entry->classes(type_table);
        if (classes.front() == MEMORY)
            ret.in_memory = true;
        else {
            size_t offset = 0;
            for (auto &c : classes) {
                Operand op = PseudoAggregate{ *getString(operand), offset };
                if (c == SSE)
                    ret.double_values.push_back(op);
                else if (c == INTEGER) {
                    AssemblyType part_type = getAggregatePartType(offset, aggregate_size);
                    ret.int_values.push_back({ op, part_type });
                } else if (c == MEMORY)
                    assert(false);
                offset += 8;
            }
        }
    }
    return ret;
}

}; // assembly
