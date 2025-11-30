#pragma once

#include "common/macro.h"
#include "common/operator.h"
#include <set>
#include <string>
#include <vector>

namespace parser {

#define TYPE_SPECIFIER_LIST(X) \
    X(TypeInt, "int")

enum TypeSpecifier {
#define ADD_TYPE_TO_ENUM(enumname, stringname) enumname,
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_ENUM)
#undef ADD_TYPE_TO_ENUM
};

#define STORAGE_CLASS_LIST(X) \
    X(StorageStatic, "static") \
    X(StorageExtern, "extern")

enum StorageClass {
    StorageDefault,
    StorageStatic,
    StorageExtern
};

#define AST_DECLARATION_LIST(X) \
    X(FunctionDeclaration, \
        StorageClass storage; \
        std::string name; \
        std::vector<std::string> params; \
        std::unique_ptr<Statement> body;) \
    X(VariableDeclaration, \
        StorageClass storage; \
        std::string identifier; \
        std::unique_ptr<Expression> init;) \

#define AST_STATEMENT_LIST(X) \
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
        std::set<int> cases; \
        bool hasDefault; \
        std::string label;) \
    X(CaseStatement, \
        std::unique_ptr<Expression> condition; \
        std::unique_ptr<Statement> statement; \
        std::string label;) \
    X(DefaultStatement, \
        std::unique_ptr<Statement> statement; \
        std::string label;) \

#define AST_EXPRESSION_LIST(X) \
    X(NumberExpression, \
        int value;) \
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
        std::unique_ptr<Expression> falseBranch;) \
    X(FunctionCallExpression, \
        std::string identifier; \
        std::vector<std::unique_ptr<Expression>> args;) \

AST_DECLARATION_LIST(FORWARD_DECL_NODE)
AST_STATEMENT_LIST(FORWARD_DECL_NODE)
AST_EXPRESSION_LIST(FORWARD_DECL_NODE)

using Declaration = std::variant<AST_DECLARATION_LIST(ADD_TO_VARIANT) std::monostate>;
using Statement = std::variant<AST_STATEMENT_LIST(ADD_TO_VARIANT) std::monostate>;
using Expression = std::variant<AST_EXPRESSION_LIST(ADD_TO_VARIANT) std::monostate>;
using BlockItem = std::variant<
    AST_DECLARATION_LIST(ADD_TO_VARIANT)
    AST_STATEMENT_LIST(ADD_TO_VARIANT)
    std::monostate
>;
using ForInit = std::variant<
    AST_DECLARATION_LIST(ADD_TO_VARIANT)
    AST_EXPRESSION_LIST(ADD_TO_VARIANT)
    std::monostate
>;

AST_DECLARATION_LIST(DEFINE_NODE)
AST_STATEMENT_LIST(DEFINE_NODE)
AST_EXPRESSION_LIST(DEFINE_NODE)

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

template <typename T>
inline BlockItem to_block_item(T &&stmt) {
    return std::visit([](auto &&s) -> BlockItem {
        return BlockItem{ std::forward<decltype(s)>(s) };
    }, std::move(stmt));
}

template <typename T>
inline ForInit to_for_init(T &&expr) {
    return std::visit([](auto &&s) -> ForInit {
        return ForInit{ std::forward<decltype(s)>(s) };
    }, std::move(expr));
}

}; // namespace parser
