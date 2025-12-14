#pragma once

#include "types.h"

// Convert the value to a given type
ConstantValue ConvertValue(const ConstantValue &v, const Type &t);
// Convert the value only if it's necessary (e.g.: long -> shorter type)
ConstantValue ConvertValueIfNeeded(const ConstantValue &v, const Type &t);
ConstantValue MakeConstantValue(long value, const Type &type);
