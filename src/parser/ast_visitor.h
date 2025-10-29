#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include "ast_nodes.h"

namespace parser {

template <typename T>
struct IASTVisitor {
    // --- Expressions ---
    virtual T operator()(const parser::NumberExpression &) = 0;
    virtual T operator()(const parser::VariableExpression &) = 0;
    virtual T operator()(const parser::BinaryExpression &) = 0;
    virtual T operator()(const parser::CallExpression &) = 0;

    // --- Statements ---
    virtual T operator()(const parser::VarDeclStatement &) = 0;
    virtual T operator()(const parser::FuncDeclStatement &) = 0;
    virtual T operator()(const parser::ExpressionStatement &) = 0;
    virtual T operator()(const parser::ReturnStatement &) = 0;
    virtual T operator()(const parser::IfStatement &) = 0;
    virtual T operator()(const parser::BlockStatement &) = 0;
};

}; // parser

#endif // AST_VISITOR_H
