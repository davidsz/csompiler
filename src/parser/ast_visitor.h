#pragma once

#include "ast_nodes.h"

namespace parser {

template <typename T>
struct IASTVisitor {
    AST_DECLARATION_LIST(ADD_TO_VISITOR)
    AST_STATEMENT_LIST(ADD_TO_VISITOR)
    AST_EXPRESSION_LIST(ADD_TO_VISITOR)
    virtual T operator()(std::monostate) = 0;
};

}; // parser
