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

int Type::getBytes()
{
    switch (*std::get_if<BasicType>(&t)) {
    case BasicType::Int:
        return 4;
    case BasicType::Long:
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

void DebugPrint(const SymbolTable &symbolTable)
{
    for (const auto &[name, entry] : symbolTable) {
        std::cout << name << " ";
        std::cout << "[" << entry.type << "] ";
        std::cout << (entry.attrs.defined ? "defined" : "undefined") << " ";
        std::cout << (entry.attrs.global ? "global" : "local") << " ";
        if (std::holds_alternative<Initial>(entry.attrs.init)) {
            auto &initial = std::get<Initial>(entry.attrs.init);
            std::cout << "Initial " << toString(initial.i);
        } else if (std::holds_alternative<Tentative>(entry.attrs.init))
            std::cout << "Tentative";
        else
            std::cout << "NoInitializer";
        std::cout << std::endl;
    }
}
