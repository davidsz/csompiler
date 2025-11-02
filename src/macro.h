#include <variant>

#define FORWARD_DECL_NODE(name, members) \
    struct name;

#define DEFINE_NODE(name, members) \
    struct name { members };

#define ADD_TO_VARIANT(name, members) name,

#define ADD_TO_VISITOR(name, members) \
    virtual T operator()(const name &) = 0;

#define DEFINE_NODES_WITH_COMMON_VARIANT(TypeName, LIST_MACRO)                                \
    LIST_MACRO(FORWARD_DECL_NODE)                                                \
    using TypeName = std::variant<LIST_MACRO(ADD_TO_VARIANT) std::monostate>;    \
    LIST_MACRO(DEFINE_NODE)
