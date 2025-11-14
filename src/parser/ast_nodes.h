#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include <string>
#include <vector>

namespace parser {

#define AST_STATEMENT_LIST(X) \
    X(FuncDeclStatement, std::string name; std::vector<std::string> params; std::vector<BlockItem> body;) \
    X(ReturnStatement, std::unique_ptr<Expression> expr;) \
    X(BlockStatement, std::vector<std::unique_ptr<Statement>> statements;) \
    X(ExpressionStatement, std::unique_ptr<Expression> expr;) \
    X(NullStatement, /* no op */)

#define AST_EXPRESSION_LIST(X) \
    X(NumberExpression, double value;) \
    X(VariableExpression, std::string identifier;) \
    X(UnaryExpression, UnaryOperator op; std::unique_ptr<Expression> expr;) \
    X(BinaryExpression, BinaryOperator op; std::unique_ptr<Expression> lhs; std::unique_ptr<Expression> rhs;) \
    X(AssignmentExpression, std::unique_ptr<Expression> lhs; std::unique_ptr<Expression> rhs;)

AST_STATEMENT_LIST(FORWARD_DECL_NODE)
AST_EXPRESSION_LIST(FORWARD_DECL_NODE)
struct Declaration;

using Statement = std::variant<AST_STATEMENT_LIST(ADD_TO_VARIANT) std::monostate>;
using Expression = std::variant<AST_EXPRESSION_LIST(ADD_TO_VARIANT) std::monostate>;
using BlockItem = std::variant<
    AST_STATEMENT_LIST(ADD_TO_VARIANT)
    Declaration,
    std::monostate
>;

AST_STATEMENT_LIST(DEFINE_NODE)
AST_EXPRESSION_LIST(DEFINE_NODE)

struct Declaration {
    std::string identifier;
    std::unique_ptr<Expression> init; // Optional
};

template <typename T, typename... Args>
std::unique_ptr<Expression> make_expression(Args&&... args) {
    return std::make_unique<Expression>(T{std::forward<Args>(args)...});
}

template <typename T, typename... Args>
std::unique_ptr<Statement> make_statement(Args&&... args) {
    return std::make_unique<Statement>(T{std::forward<Args>(args)...});
}

template <typename T>
std::unique_ptr<Statement> wrap_statement(std::unique_ptr<T> stmt) {
    return std::make_unique<Statement>(std::move(*stmt));
}

template <typename T>
Statement wrap_statement(T &&item) {
    return Statement{ std::forward<T>(item) };
}

template <typename T>
Expression wrap_expression(T &&item) {
    return Expression{ std::forward<T>(item) };
}

template <typename T>
std::unique_ptr<Expression> unique_expression(T &&item) {
    return std::make_unique<Expression>(std::forward<T>(item));
}
#define UE(x) unique_expression(std::move(x))

template <typename T>
BlockItem to_block_item(T &&item) {
    return BlockItem{ std::forward<T>(item) };
}

inline BlockItem to_block_item(Statement &&stmt) {
    return std::visit([](auto &&s) -> BlockItem {
        return BlockItem{ std::forward<decltype(s)>(s) };
    }, std::move(stmt));
}

}; // namespace parser
