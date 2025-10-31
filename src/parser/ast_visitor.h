#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include "ast_nodes.h"

namespace parser {

template <typename T>
struct IASTVisitor {
    AST_STATEMENT_LIST(ADD_TO_VISITOR)
    AST_EXPRESSION_LIST(ADD_TO_VISITOR)
    virtual T operator()(const parser::Empty &) = 0;
};

}; // parser

#endif // AST_VISITOR_H
