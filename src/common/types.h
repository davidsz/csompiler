#pragma once

#include <stddef.h>
#include <string>
#include <unordered_map>
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


// Symbol table
struct Tentative {};
struct NoInitializer {};
struct Initial {
    int i;
    auto operator<=>(const Initial &) const = default;
};
using InitialValue = std::variant<Tentative, NoInitializer, Initial>;

struct IdentifierAttributes {
    enum Type {
        Function,
        Static,
        Local
    };
    Type type = Local;
    bool defined = false;
    bool global = false;
    InitialValue init = NoInitializer{};
};

struct SymbolEntry {
    TypeInfo type;
    IdentifierAttributes attrs;
};

using SymbolTable = std::unordered_map<std::string, SymbolEntry>;

void DebugPrint(const SymbolTable &);
