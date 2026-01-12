#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

// Type and storage specifiers
#define TYPE_SPECIFIER_LIST(X) \
    X("int") \
    X("long") \
    X("signed") \
    X("unsigned") \
    X("double")

#define STORAGE_CLASS_LIST(X) \
    X(StorageStatic, "static") \
    X(StorageExtern, "extern")

enum StorageClass {
    StorageDefault,
    StorageStatic,
    StorageExtern
};

bool IsTypeSpecifier(const std::string &type);
std::optional<StorageClass> GetStorageClass(const std::string &storage);
bool IsStorageOrTypeSpecifier(const std::string &type);

// Assembly word types
enum WordType {
    Longword,
    Quadword,
    Doubleword
};

uint8_t GetBytesOfWordType(WordType type);

// Recursive type which represents types
struct Type;

enum BasicType {
    Int,
    Long,
    UInt,
    ULong,
    Double,
};

struct FunctionType {
    std::vector<std::shared_ptr<Type>> params;
    std::shared_ptr<Type> ret;
    bool operator==(const FunctionType &other) const;
};

struct PointerType {
    std::shared_ptr<Type> referenced;
    bool operator==(const PointerType &other) const;
};

struct ArrayType {
    std::shared_ptr<Type> element;
    uint64_t count;
    bool operator==(const ArrayType &other) const;
};

using TypeInfo = std::variant<
    std::monostate,
    BasicType,
    FunctionType,
    PointerType,
    ArrayType
>;

struct Type {
    TypeInfo t; // std::monostate until initialized

    template <typename T>
    T *getAs() { return std::get_if<T>(&t); }

    template <typename T>
    const T *getAs() const { return std::get_if<T>(&t); }

    bool isBasic(BasicType type) const;
    bool isFunction() const;
    bool isPointer() const;
    bool isArray() const;
    bool isInteger() const;
    bool isSigned() const;
    bool isArithmetic() const;
    bool isInitialized() const;
    int size() const;
    WordType wordType() const;

    friend bool operator==(const Type &a, const Type &b) {
        return a.t == b.t;
    }

    friend bool operator!=(const Type &a, const Type &b) {
        return !(a == b);
    }

    friend std::ostream &operator<<(std::ostream &os, const Type &t);
    std::string toString() const;
};

std::optional<Type> DetermineType(const std::set<std::string> &type_specifiers);
