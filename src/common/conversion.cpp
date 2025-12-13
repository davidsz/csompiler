#include "conversion.h"
#include <variant>

ConstantValue ConvertValueIfNeeded(const ConstantValue &v, const Type &to_type)
{
    if (std::get_if<int>(&v))
        return v;
    else if (const long *long_value = std::get_if<long>(&v)) {
        if (auto t = std::get_if<BasicType>(&to_type.t); t && *t == BasicType::Int)
            return static_cast<int>(*long_value & 0xFFFFFFFFLL);
    }
    return v;
}
