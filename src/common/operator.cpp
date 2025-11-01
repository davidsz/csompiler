#include "operator.h"
#include <cassert>
#include <unordered_map>

UnaryOperator toUnaryOperator(std::string_view s)
{
    static const std::unordered_map<std::string_view, UnaryOperator> map = {
#define ADD_TO_MAP(name, str) {str, UnaryOperator::name},
        UNARY_OPERATOR_LIST(ADD_TO_MAP)
#undef ADD_TO_MAP
    };
    if (auto it = map.find(s); it != map.end())
        return it->second;
    assert(false);
    return UnaryOperator::Negate;
}

std::string_view toString(UnaryOperator op)
{
    switch (op) {
#define CASE_TO_STRING(name, str) case UnaryOperator::name: return str;
        UNARY_OPERATOR_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}
