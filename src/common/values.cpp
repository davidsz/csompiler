#include "values.h"
#include <cstring>
#include <format>

std::string toString(const ConstantValue &v)
{
    return std::visit([](auto x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, double>) {
            // Round-trip safe double formatting
            std::array<char, 64> buf;
            auto [ptr, ec] =
                std::to_chars(buf.data(),
                              buf.data() + buf.size(),
                              x,
                              std::chars_format::general);
            return std::string(buf.data(), ptr);
        } else
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

bool isPositiveZero(const ConstantValue &v) {
    return std::visit([](const auto &x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_floating_point_v<T>) {
            uint64_t bits;
            std::memcpy(&bits, &x, sizeof(x));
            return bits == 0; // true for +0.0, false for -0.0
        } else if constexpr (std::is_integral_v<T>)
            return x == 0;
        else
            return false;
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
        default:
            return static_cast<int>(x);
        }
    }, v);
}

ConstantValue MakeConstantValue(long value, const Type &type)
{
    const BasicType *basic_type = type.getAs<BasicType>();
    if (!basic_type)
        return static_cast<int>(value);
    return MakeConstantValue(value, *basic_type);
}

ConstantValue MakeConstantValue(long value, BasicType type)
{
    switch (type) {
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
