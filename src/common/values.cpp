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
        if (x < 0) {
            x *= -1;
            return std::format("_{}", std::to_string(x));
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
