#pragma once

#include <iostream>
#include <variant>

#ifdef DLOG
    #define LOG(x) do { std::cerr << "[LOG] " << x << std::endl; } while(0)
#else
    #define LOG(x) do {} while(0)
#endif

#define FORWARD_DECL_NODE(name, members) \
    struct name;

#define DEFINE_NODE(name, members) \
    struct name { members };

#define ADD_TO_VARIANT(name, members) \
    name,

#define ADD_TO_ENUM(name, value) \
    name,

#define ADD_TO_VISITOR(name, members) \
    virtual T operator()(const name &) = 0;

#define ADD_REF_TO_VISITOR(name, members) \
    virtual T operator()(name &) = 0;

#define DEFINE_NODES_WITH_COMMON_VARIANT(TypeName, LIST_MACRO) \
    LIST_MACRO(FORWARD_DECL_NODE) \
    using TypeName = std::variant<LIST_MACRO(ADD_TO_VARIANT) std::monostate>; \
    LIST_MACRO(DEFINE_NODE)

#define DEFINE_ENUM(EnumName, LIST_MACRO) \
    enum EnumName { \
    LIST_MACRO(ADD_TO_ENUM) \
    };

#define ADD_OPERATOR_TO_ENUM(name, value, precedence, asm) \
    name,

#define ADD_ASM_OPERATOR_TO_ENUM(enum_name, text_name) \
    enum_name,

#define DEFINE_OPERATOR(EnumName, LIST_MACRO) \
    enum EnumName { \
    LIST_MACRO(ADD_OPERATOR_TO_ENUM) \
    };

#define DEFINE_ASM_OPERATOR(EnumName, LIST_MACRO) \
    enum EnumName { \
    LIST_MACRO(ADD_ASM_OPERATOR_TO_ENUM) \
    };
