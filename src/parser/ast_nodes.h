#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include <string>
#include <vector>

namespace parser {

#define AST_STATEMENT_LIST(X) \
    X(FuncDeclStatement, \
        std::string name; \
        std::vector<std::string> params; \
        std::unique_ptr<Statement> body;) \
    X(ReturnStatement, \
        std::unique_ptr<Expression> expr;) \
    X(IfStatement, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Statement> trueBranch; \
        std::unique_ptr<Statement> falseBranch;) \
    X(GotoStatement, \
        std::string label;) \
    X(LabeledStatement, \
        std::string label; \
        std::unique_ptr<Statement> statement;) \
    X(BlockStatement, \
        std::vector<BlockItem> items;) \
    X(ExpressionStatement, \
        std::unique_ptr<Expression> expr;) \
    X(NullStatement, /* no op */) \
    X(BreakStatement, \
        std::string label;) \
    X(ContinueStatement, \
        std::string label;) \
    X(WhileStatement, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Statement> body; \
        std::string label;) \
    X(DoWhileStatement, \
        std::unique_ptr<Statement> body; \
        std::unique_ptr<Expression> condition; \
        std::string label;) \
    X(ForStatement, \
        std::unique_ptr<ForInit> init; \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Expression> update; \
        std::unique_ptr<Statement> body; \
        std::string label;) \
    X(SwitchStatement, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Statement> body; \
        std::string label;) \
    X(CaseStatement, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Statement> statement;) \
    X(DefaultStatement, \
        std::unique_ptr<Statement> statement;) \

#define AST_EXPRESSION_LIST(X) \
    X(NumberExpression, \
        double value;) \
    X(VariableExpression, \
        std::string identifier;) \
    X(UnaryExpression, \
        UnaryOperator op; \
        std::unique_ptr<Expression> expr; \
        bool postfix = false;) \
    X(BinaryExpression, \
        BinaryOperator op; \
        std::unique_ptr<Expression> lhs; \
        std::unique_ptr<Expression> rhs;) \
    X(AssignmentExpression, \
        std::unique_ptr<Expression> lhs; \
        std::unique_ptr<Expression> rhs;) \
    X(ConditionalExpression, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Expression> trueBranch; \
        std::unique_ptr<Expression> falseBranch;)

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
using ForInit = std::variant<
    AST_EXPRESSION_LIST(ADD_TO_VARIANT)
    Declaration,
    std::monostate
>;

AST_STATEMENT_LIST(DEFINE_NODE)
AST_EXPRESSION_LIST(DEFINE_NODE)

struct Declaration {
    std::string identifier;
    std::unique_ptr<Expression> init; // Optional
};

template <typename T>
std::unique_ptr<Expression> unique_expression(T &&item) {
    return std::make_unique<Expression>(std::forward<T>(item));
}
#define UE(x) unique_expression(std::move(x))

template <typename T>
std::unique_ptr<Statement> unique_statement(T &&item) {
    return std::make_unique<Statement>(std::forward<T>(item));
}
#define US(x) unique_statement(std::move(x))

inline BlockItem to_block_item(Statement &&stmt) {
    return std::visit([](auto &&s) -> BlockItem {
        return BlockItem{ std::forward<decltype(s)>(s) };
    }, std::move(stmt));
}

inline ForInit to_for_init(Expression &&expr) {
    return std::visit([](auto &&s) -> ForInit {
        return ForInit{ std::forward<decltype(s)>(s) };
    }, std::move(expr));
}

}; // namespace parser
