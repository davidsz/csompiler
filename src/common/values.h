#pragma once

#include "types.h"
#include <variant>

// Holds a value for constant literals or initializers
using ConstantValue = std::variant<
    int, long
>;
std::string toString(const ConstantValue &v);
std::string toLabel(const ConstantValue &v);
Type getType(const ConstantValue &v);
long forceLong(const ConstantValue &v);
