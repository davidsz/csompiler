#pragma once

#include <stddef.h>
#include <variant>

// TODO: Wrap it later to support recursive types
// struct Type;

enum BasicType {
    Int,
};

struct FunctionType {
    size_t paramCount = 0;
    auto operator<=>(const FunctionType &) const = default;
};
using TypeInfo = std::variant<BasicType, FunctionType>;

// struct Type {
//     TypeInfo t;
// };

// TODO: Can be set of strings, the enum is unnessary
#define TYPE_SPECIFIER_LIST(X) \
    X(TypeInt, "int")

enum TypeSpecifier {
#define ADD_TYPE_TO_ENUM(enumname, stringname) enumname,
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_ENUM)
#undef ADD_TYPE_TO_ENUM
};

#define STORAGE_CLASS_LIST(X) \
    X(StorageStatic, "static") \
    X(StorageExtern, "extern")

enum StorageClass {
    StorageDefault,
    StorageStatic,
    StorageExtern
};
