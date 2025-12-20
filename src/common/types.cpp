#include "types.h"
#include <iostream>

bool FunctionType::operator==(const FunctionType &other) const
{
    if (params.size() != other.params.size())
        return false;
    for (size_t i = 0; i < params.size(); i++) {
        if (*params[i] != *other.params[i])
            return false;
    }
    return *ret == *other.ret;
}

bool Type::isBasic(BasicType type) const
{
    if (auto p = std::get_if<BasicType>(&t))
        return *p == type;
    return false;
}

bool Type::isSigned() const
{
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return false;
    switch (*basic_type) {
    case BasicType::Int:
    case BasicType::Long:
        return true;
    case BasicType::UInt:
    case BasicType::ULong:
    default:
        return false;
    }
}

int Type::size() const
{
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return 0;
    switch (*basic_type) {
    case BasicType::Int:
    case BasicType::UInt:
        return 4;
    case BasicType::Long:
    case BasicType::ULong:
        return 8;
    default:
        return 0;
    }
}

std::ostream &operator<<(std::ostream &os, const Type &type)
{
    std::visit([&](auto &obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, BasicType>)
            os << "BasicType " << (int)obj;
        else if constexpr (std::is_same_v<T, FunctionType>)
            os << "FunctionType";
        else
            os << "typeless";
    }, type.t);
    return os;
}
