#define FORWARD_DECL_NODE(name, members) \
    struct name;

#define DEFINE_NODE(name, members) \
    struct name { members };

#define ADD_TO_VARIANT(name, members) name,

#define ADD_TO_VISITOR(name, members) \
    virtual T operator()(const name &) = 0;
