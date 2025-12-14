#include "conversion.h"
#include <variant>

ConstantValue ConvertValue(const ConstantValue &v, const Type &to_type)
{
    if (const int *int_value = std::get_if<int>(&v)) {
        if (to_type.isBasic(BasicType::Long))
            return static_cast<long>(*int_value);
    } else if (const long *long_value = std::get_if<long>(&v)) {
        if (to_type.isBasic(BasicType::Int))
            return static_cast<int>(*long_value & 0xFFFFFFFFLL);
    }
    return v;
}

ConstantValue ConvertValueIfNeeded(const ConstantValue &v, const Type &to_type)
{
    if (std::get_if<int>(&v)) {
        return v;
    } else if (const long *long_value = std::get_if<long>(&v)) {
        if (to_type.isBasic(BasicType::Int))
            return static_cast<int>(*long_value & 0xFFFFFFFFLL);
    }
    return v;
}

ConstantValue MakeConstantValue(long value, const Type &type)
{
    if (type.isBasic(BasicType::Int))
        return static_cast<int>(value);
    else if (type.isBasic(BasicType::Long))
        return static_cast<long>(value);
    return static_cast<int>(value);
}
