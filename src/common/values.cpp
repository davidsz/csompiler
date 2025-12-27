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
    if (std::holds_alternative<int>(v)) {
        ret.t = BasicType::Int;
    } else if (std::holds_alternative<long>(v)) {
        ret.t = BasicType::Long;
    } else if (std::holds_alternative<uint32_t>(v)) {
        ret.t = BasicType::UInt;
    } else if (std::holds_alternative<uint64_t>(v)) {
        ret.t = BasicType::ULong;
    } else if (std::holds_alternative<double>(v)) {
        ret.t = BasicType::Double;
    }
    return ret;
}

bool fitsLongWord(const ConstantValue &v)
{
    if (std::holds_alternative<int>(v) ||
        std::holds_alternative<uint32_t>(v)) {
        return true;
    } else if (std::holds_alternative<long>(v)
        || std::holds_alternative<uint64_t>(v)
        || std::holds_alternative<double>(v)) {
        return false;
    }
    return false;
}

uint64_t forceLong(const ConstantValue &v)
{
    return std::visit([&](auto value) {
        return static_cast<uint64_t>(value);
    }, v);
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
        case BasicType::Double:
            return static_cast<double>(x);
        }
    }, v);
}

ConstantValue MakeConstantValue(long value, const Type &type)
{
    const BasicType *basic_type = type.getAs<BasicType>();
    if (!basic_type)
        return static_cast<int>(value);
    switch (*basic_type) {
    case BasicType::Int:
        return static_cast<int>(value);
    case BasicType::Long:
        return static_cast<long>(value);
    case BasicType::UInt:
        return static_cast<uint32_t>(value);
    case BasicType::ULong:
        return static_cast<uint64_t>(value);
    case BasicType::Double:
        return static_cast<double>(value);
    }
    return static_cast<int>(value);
}
