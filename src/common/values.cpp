#include "values.h"
#include "common/system.h"
#include <cassert>
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
        } else if constexpr (std::is_same_v<T, ZeroBytes>)
            return std::format("ZeroBytes[{}]", x.bytes);
        else if constexpr (std::is_same_v<T, StringInit>)
            return std::format("StringInit[{}]", x.text);
        else if constexpr (std::is_same_v<T, PointerInit>)
            return std::format("PointerInit[{}]", x.name);
        else
            return std::to_string(x);
    }, v);
}

std::string toLabel(const ConstantValue &v)
{
    return std::visit([](auto x) -> std::string {
        using T = decltype(x);
        if constexpr (std::is_signed_v<T>) {
            if (x < 0) {
                x *= -1;
                return std::format("_{}", std::to_string(x));
            }
            return std::to_string(x);
        } else if constexpr (is_custom_constant_type_v<T>) {
            assert(false);
            return "";
        } else
            return std::to_string(x);
    }, v);
}

Type getType(const ConstantValue &v)
{
    Type ret;
    ret.t = std::visit([](auto x) -> BasicType {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, int>) return BasicType::Int;
        if constexpr (std::is_same_v<T, long>) return BasicType::Long;
        if constexpr (std::is_same_v<T, uint32_t>) return BasicType::UInt;
        if constexpr (std::is_same_v<T, uint64_t>) return BasicType::ULong;
        if constexpr (std::is_same_v<T, double>) return BasicType::Double;
        if constexpr (std::is_same_v<T, char>) return BasicType::Char;
        if constexpr (std::is_same_v<T, unsigned char>) return BasicType::UChar;
        else assert(false);
    }, v);
    return ret;
}

bool isPositiveZero(const ConstantValue &v)
{
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

bool isZero(const ConstantValue &value)
{
    return std::visit([](const auto &v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_arithmetic_v<T>)
            return v == T(0);
        return false;
    }, value);
}

bool isNan(const ConstantValue &value)
{
    return std::visit([](const auto &v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>)
            return std::isnan(v);
        return false;
    }, value);
}

size_t byteSizeOf(const ConstantValue &c)
{
    return std::visit([](auto &&v) -> size_t {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, ZeroBytes>)
            return v.bytes;
        else if constexpr (std::is_same_v<V, StringInit>)
            return v.text.size() + (v.null_terminated ? 1 : 0);
        if constexpr (std::is_same_v<V, PointerInit>)
            return 8;
        else
            return sizeof(V);
    }, c);
}

ConstantValue ConvertValue(const ConstantValue &v, const Type &to_type)
{
    return std::visit([&](const auto &x) -> ConstantValue {
        using T = std::decay_t<decltype(x)>;
        if constexpr (is_custom_constant_type_v<T>) {
            assert(false);
            return x;
        } else {
            if (to_type.isPointer())
                return static_cast<unsigned long>(x);
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
            case BasicType::Char:
            case BasicType::SChar:
                return static_cast<char>(x);
            case BasicType::UChar:
                return static_cast<unsigned char>(x);
            default:
                return static_cast<int>(x);
            }
        }
    }, v);
}

ConstantValue MakeConstantValue(long value, const Type &type)
{
    if (const BasicType *basic_type = type.getAs<BasicType>())
        return MakeConstantValue(value, *basic_type);
    if (type.isPointer())
        return MakeConstantValue(value, ULong);
    assert(false);
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
    case BasicType::Char:
    case BasicType::SChar:
        return static_cast<char>(value);
    case BasicType::UChar:
        return static_cast<unsigned char>(value);
    }
    return static_cast<int>(value);
}

DIAG_PUSH
DIAG_IGNORE("-Wconversion")
DIAG_IGNORE("-Wsign-compare")
bool operator==(const ConstantValue &a, const ConstantValue &b)
{
    if (a.index() != b.index())
        return false;
    return std::visit([](const auto &x, const auto &y) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (
            std::is_same_v<T, int> ||
            std::is_same_v<T, long> ||
            std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, uint64_t> ||
            std::is_same_v<T, char> ||
            std::is_same_v<T, unsigned char>
        ) {
            return x == y;
        } else if constexpr (std::is_same_v<T, double>) {
            using U = std::decay_t<decltype(y)>;
            if constexpr (std::is_same_v<U, double>)
                return std::bit_cast<uint64_t>(x) == std::bit_cast<uint64_t>(y);
            else
                return false;
        } else {
            assert(false);
            return false;
        }
    }, a, b);
}

bool operator<(const ConstantValue &a, const ConstantValue &b)
{
    if (a.index() < b.index())
        return true;
    return std::visit([](const auto &x, const auto &y) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (
            std::is_same_v<T, int> ||
            std::is_same_v<T, long> ||
            std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, uint64_t> ||
            std::is_same_v<T, char> ||
            std::is_same_v<T, unsigned char>
        ) {
            return x < y;
        } else if constexpr (std::is_same_v<T, double>) {
            using U = std::decay_t<decltype(y)>;
            if constexpr (std::is_same_v<U, double>)
                return std::bit_cast<uint64_t>(x) < std::bit_cast<uint64_t>(y);
            else
                return false;
        } else {
            assert(false);
            return false;
        }
    }, a, b);
}
DIAG_POP
