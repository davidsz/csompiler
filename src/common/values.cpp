#include "values.h"
#include <format>

std::string toString(const ConstantValue &v)
{
    return std::visit([](auto x) {
        return std::to_string(x);
    }, v);
}

std::string toLabel(const ConstantValue &v)
{
    return std::visit([](auto x) {
        using T = decltype(x);
        if constexpr (std::is_signed_v<T>) {
            if (x < 0) {
                x *= -1;
                return std::format("_{}", std::to_string(x));
            }
        }
        return std::to_string(x);
    }, v);
}

Type getType(const ConstantValue &v)
{
    Type ret;
    switch (v.index()) {
        case 0:  ret.t = BasicType::Int; break;
        case 1:  ret.t = BasicType::Long; break;
        case 2:  ret.t = BasicType::UInt; break;
        case 3:  ret.t = BasicType::ULong; break;
    }
    return ret;
}

long forceLong(const ConstantValue &v)
{
    long ret = 0;
    if (auto int_value = std::get_if<int>(&v))
        ret = *int_value;
    else if (auto long_value = std::get_if<long>(&v))
        ret = *long_value;
    return ret;
}

ConstantValue ConvertValue(const ConstantValue &v, const Type &to_type)
{
    return std::visit([&](auto x) -> ConstantValue {
        const BasicType *basic_type = to_type.getAs<BasicType>();
        if (!basic_type)
            return v;
        switch (*basic_type) {
        case BasicType::Int:
            return static_cast<int>(x);
        case BasicType::Long:
            return static_cast<long>(x);
        case BasicType::UInt:
            return static_cast<unsigned int>(x);
        case BasicType::ULong:
            return static_cast<unsigned long>(x);
        }
    }, v);
}

ConstantValue MakeConstantValue(long value, const Type &type)
{
    if (type.isBasic(BasicType::Int))
        return static_cast<int>(value);
    else if (type.isBasic(BasicType::Long))
        return static_cast<long>(value);
    return static_cast<int>(value);
}
