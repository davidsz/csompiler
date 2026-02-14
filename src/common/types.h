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
    X("double") \
    X("char") \
    X("void") \
    X("struct")

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
// ---

// Assembly types
struct AssemblyType;

enum WordType {
    Byte,
    Longword,
    Quadword,
    Doubleword
};

struct ByteArray {
    size_t size;
    int alignment;
};

using AssemblyTypeInfo = std::variant<WordType, ByteArray>;

struct AssemblyType {
    AssemblyTypeInfo t;

    template <typename T>
    T *getAs() { return std::get_if<T>(&t); }

    template <typename T>
    const T *getAs() const { return std::get_if<T>(&t); }

    bool isWord(WordType type) const;
    bool isByteArray() const;
    int size() const;
    int alignment() const;
};

// TODO: Remove this and replace with .size() when possible
uint8_t GetBytesOfWordType(WordType type);
//---

// Recursive type structure which represents higher level type information
struct Type;

enum BasicType {
    Int,
    Long,
    UInt,
    ULong,
    Double,
    Char,
    SChar,
    UChar
};

struct FunctionType {
    std::vector<std::shared_ptr<Type>> params;
    std::shared_ptr<Type> ret;
    bool operator==(const FunctionType &other) const;
};

struct PointerType {
    std::shared_ptr<Type> referenced;
    bool decayed = false;
    bool operator==(const PointerType &other) const;
};

struct ArrayType {
    std::shared_ptr<Type> element;
    uint64_t count;
    bool operator==(const ArrayType &other) const;
};

struct VoidType {
    bool operator==(const VoidType &other) const;
};

struct StructType {
    std::string tag;
    bool operator==(const StructType &other) const;
};

using TypeInfo = std::variant<
    std::monostate,
    BasicType,
    FunctionType,
    PointerType,
    ArrayType,
    VoidType,
    StructType
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
    bool isVoid() const;
    bool isVoidPointer() const;
    bool isArray() const;
    bool isInteger() const;
    bool isComplete() const;
    bool isCompletePointer() const;
    bool isScalar() const;
    bool isSigned() const;
    bool isArithmetic() const;
    bool isCharacter() const;
    bool isInitialized() const;
    size_t size() const;
    int alignment() const;
    WordType wordType() const;
    Type storedType() const;
    Type promotedType() const;

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
