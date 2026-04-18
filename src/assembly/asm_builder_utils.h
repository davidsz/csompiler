#pragma once

#include "asm_nodes.h"
#include "common/type_table.h"
#include "tac/tac_nodes.h"

namespace assembly {

const std::string *getString(const tac::Value &value);
const std::string *getString(const Operand &op);

AssemblyType getAggregatePartType(size_t offset, size_t size);

struct ClassifiedParams {
    std::vector<std::pair<Operand, AssemblyType>> int_regs;
    std::vector<Operand> double_regs;
    std::vector<std::pair<Operand, AssemblyType>> stack;
};

template<typename T, typename GetTypeFn, typename GetOperandFn>
ClassifiedParams classifyParameters(
    const std::vector<T> &parameters,
    bool return_in_memory,
    TypeTable *type_table,
    GetTypeFn getType,
    GetOperandFn getOperand)
{
    size_t int_regs_available = return_in_memory ? 5 : 6;
    ClassifiedParams result;
    for (const auto &p : parameters) {
        Operand operand = getOperand(p);
        Type type = getType(p);
        const AggregateType *aggr_type = type.getAs<AggregateType>();
        if (!aggr_type) {
            WordType word_type = type.wordType();
            if (word_type == WordType::Doubleword) {
                if (result.double_regs.size() < 8)
                    result.double_regs.push_back(operand);
                else
                    result.stack.push_back({
                        operand,
                        AssemblyType{ word_type }
                    });
            } else {
                if (result.int_regs.size() < int_regs_available)
                    result.int_regs.push_back({
                        operand,
                        AssemblyType{ word_type }
                    });
                else
                    result.stack.push_back({
                        operand,
                        AssemblyType{ word_type }
                    });
            }
            continue;
        }

        // Parameter is an aggregate type
        size_t aggregate_size = type.size(type_table);
        TypeTable::AggregateEntry *entry = type_table->get(aggr_type->tag);
        auto &classes = entry->classes(type_table);
        bool use_stack = true;
        if (classes.front() != MEMORY) {
            std::vector<std::pair<Operand, AssemblyType>> tentative_ints;
            std::vector<Operand> tentative_doubles;
            size_t offset = 0;
            for (auto &c : classes) {
                Operand pseudo = PseudoAggregate{ *getString(operand), offset };
                if (c == SSE)
                    tentative_doubles.push_back(pseudo);
                else {
                    AssemblyType part_type = getAggregatePartType(offset, aggregate_size);
                    tentative_ints.push_back({ pseudo, part_type });
                }
                offset += 8;
            }

            if ((tentative_ints.size() + result.int_regs.size() <= int_regs_available)
                && (tentative_doubles.size() + result.double_regs.size() <= 8)) {
                result.int_regs.insert(result.int_regs.end(),
                    tentative_ints.begin(),
                    tentative_ints.end());
                result.double_regs.insert(result.double_regs.end(),
                    tentative_doubles.begin(),
                    tentative_doubles.end());
                use_stack = false;
            }
        }

        if (use_stack) {
            size_t offset = 0;
            for (auto &c : classes) {
                Operand pseudo = PseudoAggregate{ *getString(operand), offset };
                AssemblyType part_type;
                if (c == SSE)
                    part_type = AssemblyType{ WordType::Doubleword };
                else
                    part_type = getAggregatePartType(offset, aggregate_size);
                result.stack.push_back({ pseudo, part_type });
                offset += 8;
            }
        }
    }
    return result;
}

struct ClassifiedReturn {
    std::vector<std::pair<Operand, AssemblyType>> int_values;
    std::vector<Operand> double_values;
    bool in_memory = false;
};

ClassifiedReturn classifyReturnValue(
    const Operand &operand,
    const Type &type,
    TypeTable *type_table);

}; // assembly
