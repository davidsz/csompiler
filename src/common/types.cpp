#include "types.h"
#include <cassert>
#include <iostream>
#include <sstream>
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

static std::string toString(BasicType type)
{
    switch (type) {
    case BasicType::Int:
        return "int";
    case BasicType::UInt:
        return "unsigned int";
    case BasicType::Long:
        return "long";
    case BasicType::ULong:
        return "unsigned long";
    case BasicType::Double:
        return "double";
    case BasicType::Char:
        return "char";
    case BasicType::SChar:
        return "signed char";
    case BasicType::UChar:
        return "unsigned char";
    default:
        return "unknown";
    }
}

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

    if (type_specifiers.contains("void")) {
        if (type_specifiers.size() != 1)
            return std::nullopt;
        return Type { VoidType{} };
    }

    if (type_specifiers.contains("char")) {
        if (type_specifiers.size() == 1)
            return Type { BasicType::Char };
        else if (type_specifiers.size() == 2 && type_specifiers.contains("unsigned"))
            return Type { BasicType::UChar };
        else if (type_specifiers.size() == 2 && type_specifiers.contains("signed"))
            return Type { BasicType::SChar };
        else
            return std::nullopt;
    }

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

bool AssemblyType::isWord(WordType type) const
{
    if (auto w = std::get_if<WordType>(&t))
        return *w == type;
    return false;
}

bool AssemblyType::isByteArray() const
{
    return std::holds_alternative<ByteArray>(t);
}

int AssemblyType::size() const
{
    if (auto b = std::get_if<ByteArray>(&t))
        return static_cast<int>(b->size);
    auto w = std::get_if<WordType>(&t);
    assert(w);
    switch (*w) {
    case WordType::Byte:
        return 1;
    case WordType::Longword:
        return 4;
    case WordType::Quadword:
    case WordType::Doubleword:
        return 8;
    default:
        assert(false);
        return 1;
    }
}

int AssemblyType::alignment() const
{
    if (auto b = std::get_if<ByteArray>(&t))
        return b->alignment;
    return size();
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

bool PointerType::operator==(const PointerType &other) const
{
    return *referenced == *other.referenced;
}

bool ArrayType::operator==(const ArrayType &other) const
{
    return *element == *other.element && count == other.count;
}

bool VoidType::operator==(const VoidType &) const
{
    return true;
}

bool Type::isBasic(BasicType type) const
{
    if (auto p = std::get_if<BasicType>(&t))
        return *p == type;
    return false;
}

bool Type::isFunction() const
{
    return std::holds_alternative<FunctionType>(t);
}

bool Type::isPointer() const
{
    return std::holds_alternative<PointerType>(t);
}

bool Type::isVoid() const
{
    return std::holds_alternative<VoidType>(t);
}

bool Type::isVoidPointer() const
{
    if (auto pointer_type = std::get_if<PointerType>(&t))
        return pointer_type->referenced->isVoid();
    return false;
}

bool Type::isArray() const
{
    return std::holds_alternative<ArrayType>(t);
}

bool Type::isInteger() const
{
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return false;
    switch (*basic_type) {
    case BasicType::Int:
    case BasicType::UInt:
    case BasicType::Long:
    case BasicType::ULong:
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return true;
    case BasicType::Double:
    default:
        return false;
    }
}

bool Type::isComplete() const
{
    return !isVoid();
}

bool Type::isCompletePointer() const
{
    if (auto pointer_type = std::get_if<PointerType>(&t))
        return pointer_type->referenced->isComplete();
    return false;
}

bool Type::isScalar() const
{
    return !isVoid() && !isArray() && !isFunction();
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
    case BasicType::Char:
    case BasicType::SChar:
        return true;
    case BasicType::UInt:
    case BasicType::ULong:
    case BasicType::UChar:
    default:
        return false;
    }
}

bool Type::isArithmetic() const
{
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return false;
    switch (*basic_type) {
    case BasicType::Int:
    case BasicType::Long:
    case BasicType::UInt:
    case BasicType::ULong:
    case BasicType::Double:
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return true;
    default:
        return false;
    }
}

bool Type::isCharacter() const
{
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return false;
    switch (*basic_type) {
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return true;
    case BasicType::Int:
    case BasicType::Long:
    case BasicType::UInt:
    case BasicType::ULong:
    case BasicType::Double:
    default:
        return false;
    }
}

bool Type::isInitialized() const
{
    return !std::holds_alternative<std::monostate>(t);
}

size_t Type::size() const
{
    if (isPointer())
        return 8;
    if (const ArrayType *arr = std::get_if<ArrayType>(&t))
        return arr->element->size() * arr->count;
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    if (!basic_type)
        return 0;
    switch (*basic_type) {
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return 1;
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

int Type::alignment() const
{
    size_t size = this->size();
    if (size > 16)
        return 16;
    if (auto array_type = std::get_if<ArrayType>(&t)) {
        size_t element_size = array_type->element->size();
        return static_cast<int>(element_size % 2 ? element_size + 1 : element_size);
    }
    return static_cast<int>(size);
}

WordType Type::wordType() const
{
    if (std::holds_alternative<PointerType>(t))
        return Quadword;
    if (const ArrayType *array_type = std::get_if<ArrayType>(&t)) {
        assert(array_type->element->isCharacter());
        return Byte;
    }
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    assert(basic_type);
    switch (*basic_type) {
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return Byte;
    case BasicType::Int:
    case BasicType::UInt:
        return Longword;
    case BasicType::Long:
    case BasicType::ULong:
        return Quadword;
    case BasicType::Double:
        return Doubleword;
    default:
        assert(false);
        return Longword;
    }
}

Type Type::storedType() const
{
    if (auto a = std::get_if<ArrayType>(&t))
        return a->element->storedType();
    else
        return *this;
}

Type Type::promotedType() const
{
    if (isPointer())
        return *this;
    const BasicType *basic_type = std::get_if<BasicType>(&t);
    assert(basic_type);
    switch (*basic_type) {
    case BasicType::Int:
    case BasicType::UInt:
    case BasicType::Long:
    case BasicType::ULong:
        return *this;
    case BasicType::Char:
    case BasicType::SChar:
    case BasicType::UChar:
        return Type{ BasicType::Int };
    case BasicType::Double:
    default:
        return *this;
    }
}

std::ostream &operator<<(std::ostream &os, const Type &type)
{
    std::visit([&](auto &obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, BasicType>)
            os << toString(obj);
        else if constexpr (std::is_same_v<T, FunctionType>) {
            os << "FunctionType(";
            for (auto &p : obj.params)
                os << *p << ", ";
            os << ") -> " << *obj.ret;
        } else if constexpr (std::is_same_v<T, PointerType>) {
            os << "PointerType(" << *obj.referenced << ")";
            if (obj.decayed)
                os << " [decayed]";
        } else if constexpr (std::is_same_v<T, ArrayType>)
            os << "ArrayType(" << *obj.element << ")[" << obj.count << "]";
        else if constexpr (std::is_same_v<T, VoidType>)
            os << "VoidType";
        else
            os << "typeless";
    }, type.t);
    return os;
}

std::string Type::toString() const
{
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

uint8_t GetBytesOfWordType(WordType type)
{
    switch (type) {
    case WordType::Byte:
        return 1;
    case WordType::Longword:
        return 4;
    case WordType::Quadword:
    case WordType::Doubleword:
        return 8;
    default:
        assert(false);
        return 1;
    }
}
