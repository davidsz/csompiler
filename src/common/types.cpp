#include "types.h"
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

static const std::unordered_set<std::string> s_typeSpecifiers {
#define ADD_TYPE_TO_SET(stringname) stringname,
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_SET)
#undef ADD_TYPE_TO_SET
};

static const std::unordered_map<std::string, StorageClass> s_storageClasses {
#define ADD_CLASS_TO_MAP(enumname, stringname) {stringname, enumname},
    STORAGE_CLASS_LIST(ADD_CLASS_TO_MAP)
#undef ADD_CLASS_TO_MAP
};

static const std::unordered_set<std::string> s_specifiers {
#define ADD_TYPE_TO_SET(stringname) stringname,
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_SET)
#undef ADD_TYPE_TO_SET
#define ADD_CLASS_TO_SET(enumname, stringname) stringname,
    STORAGE_CLASS_LIST(ADD_CLASS_TO_SET)
#undef ADD_CLASS_TO_SET
};

bool IsTypeSpecifier(const std::string &type)
{
    return s_typeSpecifiers.contains(type);
}

std::optional<StorageClass> GetStorageClass(const std::string &storage)
{
    auto it = s_storageClasses.find(storage);
    if (it != s_storageClasses.end())
        return it->second;
    return std::nullopt;
}

bool IsStorageOrTypeSpecifier(const std::string &type)
{
    return s_specifiers.contains(type);
}

std::optional<Type> DetermineType(const std::set<std::string> &type_specifiers)
{
    if (type_specifiers.empty())
        return std::nullopt;
    if (type_specifiers.contains("signed") && type_specifiers.contains("unsigned"))
        return std::nullopt;
    if (type_specifiers.contains("double") && type_specifiers.size() > 1)
        return std::nullopt;

    if (type_specifiers.contains("unsigned") && type_specifiers.contains("long"))
        return Type { BasicType::ULong };
    else if (type_specifiers.contains("unsigned"))
        return Type { BasicType::UInt };
    else if (type_specifiers.contains("long"))
        return Type { BasicType::Long };
    else if (type_specifiers.contains("double"))
        return Type { BasicType::Double };
    else
        return Type { BasicType::Int };
}

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
    case BasicType::Double:
        return true;
    case BasicType::UInt:
    case BasicType::ULong:
    default:
        return false;
    }
}

bool Type::isInitialized() const
{
    return !std::holds_alternative<std::monostate>(t);
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
    case BasicType::Double:
        return 8;
    default:
        return 0;
    }
}

 WordType Type::wordType() const
 {
     const BasicType *basic_type = std::get_if<BasicType>(&t);
     assert(basic_type);
     switch (*basic_type) {
     case BasicType::Int:
     case BasicType::UInt:
         return Longword;
     case BasicType::Long:
     case BasicType::ULong:
        return Quadword;
     case BasicType::Double:
         return Doubleword;
     default:
         return Longword;
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

// TODO: If we only use this for registers, consider storing
// WordType in the Reg structure instead of the bytes.
uint8_t GetBytesOfWordType(WordType type)
{
    switch (type) {
    case WordType::Longword:
        return 4;
    case WordType::Quadword:
    case WordType::Doubleword:
        return 8;
    default:
        return 1;
    }
}
