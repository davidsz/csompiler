#pragma once

#include "types.h"
#include <variant>

struct ZeroBytes {
    size_t bytes;
    auto operator<=>(const ZeroBytes &) const = default;
};
struct StringInit {
    std::string text;
    bool null_terminated;
    auto operator<=>(const StringInit &) const = default;
};
struct PointerInit {
    std::string name;
    auto operator<=>(const PointerInit &) const = default;
};

template <typename T>
struct is_custom_constant_type : std::false_type {};
template <> struct is_custom_constant_type<ZeroBytes>  : std::true_type {};
template <> struct is_custom_constant_type<StringInit> : std::true_type {};
template <> struct is_custom_constant_type<PointerInit>: std::true_type {};

template <typename T>
inline constexpr bool is_custom_constant_type_v =
    is_custom_constant_type<T>::value;

using ConstantValue = std::variant<
    int, long, uint32_t, uint64_t, double, char, unsigned char
    // Only for static initializers
    , ZeroBytes      // Padding
    , StringInit     // Constant strings and char arrays
    , PointerInit    // Initialize with the address of another static object
>;

std::string toString(const ConstantValue &v);
std::string toLabel(const ConstantValue &v);
Type getType(const ConstantValue &v);
bool isPositiveZero(const ConstantValue &v);
size_t byteSizeOf(const ConstantValue &v);

// TODO: Implement a real getAs with pointer return
template <typename T>
T castTo(const ConstantValue &v) {
    return std::visit([&](auto value) -> T {
        using V = std::decay_t<decltype(value)>;
        if constexpr (is_custom_constant_type_v<V>)
            return 0;
        else
            return static_cast<T>(value);
    }, v);
}

ConstantValue ConvertValue(const ConstantValue &v, const Type &t);
ConstantValue MakeConstantValue(long value, const Type &type);
ConstantValue MakeConstantValue(long value, BasicType type);
