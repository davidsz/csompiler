#pragma once

#include "types.h"
#include <variant>

// Constant literals and initializers
struct ZeroBytes{
    size_t bytes;
    auto operator<=>(const ZeroBytes &) const = default;
};
using ConstantValue = std::variant<
    int, long, uint32_t, uint64_t, double, ZeroBytes
>;

std::string toString(const ConstantValue &v);
std::string toLabel(const ConstantValue &v);
Type getType(const ConstantValue &v);
bool fitsLongWord(const ConstantValue &v);
// TODO: Replace it by getAs<long>()
uint64_t forceLong(const ConstantValue &v);
bool isPositiveZero(const ConstantValue &v);

template <typename T>
T getAs(const ConstantValue &v) {
    return std::visit([&](auto value) -> T {
        using V = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<V, ZeroBytes>)
            return 0;
        else
            return static_cast<T>(value);
    }, v);
}

ConstantValue ConvertValue(const ConstantValue &v, const Type &t);
ConstantValue MakeConstantValue(long value, const Type &type);
ConstantValue MakeConstantValue(long value, BasicType type);
