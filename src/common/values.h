#pragma once

#include "types.h"
#include <variant>

// Holds a value for constant literals or initializers
using ConstantValue = std::variant<
    int, long, uint32_t, uint64_t, double
>;
std::string toString(const ConstantValue &v);
std::string toLabel(const ConstantValue &v);
Type getType(const ConstantValue &v);
bool fitsLongWord(const ConstantValue &v);
uint64_t forceLong(const ConstantValue &v);

ConstantValue ConvertValue(const ConstantValue &v, const Type &t);
ConstantValue MakeConstantValue(long value, const Type &type);
