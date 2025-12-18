#pragma once

#include <string>
#include <variant>
#include <vector>

// Recursive type which represents types
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

// Type and storage specifiers
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
