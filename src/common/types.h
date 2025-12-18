#pragma once

#include <stddef.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct Type;

enum BasicType {
    Int,
    Long,
};

struct FunctionType {
    std::vector<std::shared_ptr<Type>> params;
    std::shared_ptr<Type> ret;
    bool operator==(const FunctionType &other) const;
};

using TypeInfo = std::variant<
    std::monostate,
    BasicType,
    FunctionType
>;
struct Type {
    TypeInfo t; // std::monostate until initialized

    template <typename T>
    T *getAs() { return std::get_if<T>(&t); }

    template <typename T>
    const T *getAs() const { return std::get_if<T>(&t); }

    bool isBasic(BasicType type) const;
    bool isInitialized() const { return !std::holds_alternative<std::monostate>(t); }
    int getBytes();

    friend bool operator==(const Type &a, const Type &b) {
        return a.t == b.t;
    }

    friend bool operator!=(const Type &a, const Type &b) {
        return !(a == b);
    }

    friend std::ostream &operator<<(std::ostream &os, const Type &t);
};

using ConstantValue = std::variant<
    int, long
>;
std::string toString(const ConstantValue &v);
std::string toLabel(const ConstantValue &v);
Type getType(const ConstantValue &v);
long forceLong(const ConstantValue &v);

#define TYPE_SPECIFIER_LIST(X) \
    X(Int, "int") \
    X(Long, "long")

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
    ConstantValue i;
    auto operator<=>(const Initial &) const = default;
};
using InitialValue = std::variant<Tentative, NoInitializer, Initial>;

struct IdentifierAttributes {
    enum AttrType {
        Function,
        Static,
        Local
    };
    AttrType type = Local;
    bool defined = false;
    bool global = false;
    InitialValue init = NoInitializer{};
};

struct SymbolEntry {
    Type type;
    IdentifierAttributes attrs;
};

using SymbolTable = std::unordered_map<std::string, SymbolEntry>;

void DebugPrint(const SymbolTable &);
