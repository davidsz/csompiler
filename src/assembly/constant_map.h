#pragma once

#include "common/values.h"
#include <map>

// In case of floating point types, +0.0 and -0.0 should be separate keys
struct ConstantValueComparator {
    bool operator()(const ConstantValue &a, const ConstantValue &b) const {
        return std::visit([](const auto &x, const auto &y) {
            using X = std::decay_t<decltype(x)>;
            using Y = std::decay_t<decltype(y)>;
            if constexpr (!std::is_same_v<X, Y>)
                return typeid(X).before(typeid(Y));
            if constexpr (std::is_floating_point_v<X>) {
                uint64_t bx, by;
                std::memcpy(&bx, &x, sizeof(x));
                std::memcpy(&by, &y, sizeof(y));
                return bx < by;
            } else
                return x < y;
        }, a, b);
    }
};

using ConstantMap = std::map<ConstantValue, std::string, ConstantValueComparator>;
