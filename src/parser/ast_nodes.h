#ifndef AST_NODES_H
#define AST_NODES_H

#include "common/operator.h"
#include "macro.h"
#include <string>
#include <variant>
#include <vector>

namespace parser {

struct Empty {};

#define AST_STATEMENT_LIST(X) \
    X(FuncDeclStatement, std::string name; std::vector<std::string> params; std::unique_ptr<Statement> body;) \
    X(ReturnStatement, std::unique_ptr<Expression> expr;) \
    X(BlockStatement, std::vector<std::unique_ptr<Statement>> statements;)

#define AST_EXPRESSION_LIST(X) \
    X(NumberExpression, double value;) \
    X(UnaryExpression, UnaryOperator op; std::unique_ptr<Expression> expr;)


AST_EXPRESSION_LIST(FORWARD_DECL_NODE)
using Expression = std::variant<
    AST_EXPRESSION_LIST(ADD_TO_VARIANT)
    Empty
>;
AST_EXPRESSION_LIST(DEFINE_NODE)

template <typename T, typename... Args>
std::unique_ptr<Expression> make_expression(Args&&... args) {
    return std::make_unique<Expression>(T{std::forward<Args>(args)...});
}

AST_STATEMENT_LIST(FORWARD_DECL_NODE)
using Statement = std::variant<
    AST_STATEMENT_LIST(ADD_TO_VARIANT)
    Empty
>;
AST_STATEMENT_LIST(DEFINE_NODE)

template <typename T, typename... Args>
std::unique_ptr<Statement> make_statement(Args&&... args) {
    return std::make_unique<Statement>(T{std::forward<Args>(args)...});
}

template <typename T>
std::unique_ptr<Statement> wrap_statement(std::unique_ptr<T> stmt) {
    return std::make_unique<Statement>(std::move(*stmt));
}

}; // namespace parser

#endif // AST_NODES_H
