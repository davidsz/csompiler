#ifndef AST_NODES_H
#define AST_NODES_H

#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace parser {

// Expressions

struct NumberExpression;
struct VariableExpression;
struct BinaryExpression;
struct CallExpression;

using Expression = std::variant<
    NumberExpression,
    VariableExpression,
    BinaryExpression,
    CallExpression
>;

struct NumberExpression {
    double value;
};

struct VariableExpression {
    std::string name;
};

struct BinaryExpression {
    std::string op;
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;
};

struct CallExpression {
    std::string callee;
    std::vector<std::unique_ptr<Expression>> args;
};


// Statements

struct VarDeclStatement;
struct ExpressionStatement;
struct ReturnStatement;
struct IfStatement;
struct BlockStatement;

using Statement = std::variant<
    VarDeclStatement,
    ExpressionStatement,
    ReturnStatement,
    IfStatement,
    BlockStatement
>;

template <typename T, typename... Args>
std::unique_ptr<Expression> make_expression(Args&&... args) {
    return std::make_unique<Expression>(T{std::forward<Args>(args)...});
}


struct VarDeclStatement {
    std::string name;
    std::unique_ptr<Expression> init;
};

struct ExpressionStatement {
    std::unique_ptr<Expression> expr;
};

struct ReturnStatement {
    std::unique_ptr<Expression> expr;
};

struct IfStatement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;
};

struct BlockStatement {
    std::vector<std::unique_ptr<Statement>> statements;
};

template <typename T, typename... Args>
std::unique_ptr<Statement> make_statement(Args&&... args) {
    return std::make_unique<Statement>(T{std::forward<Args>(args)...});
}

template <typename T>
std::unique_ptr<Statement> wrap_statement(std::unique_ptr<T> stmt) {
    return std::make_unique<Statement>(std::move(*stmt));
}


struct FunctionDeclaration {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStatement> body;
};

using OuterNode = std::variant<
    VarDeclStatement,
    ExpressionStatement,
    ReturnStatement,
    IfStatement,
    BlockStatement,
    FunctionDeclaration
>;

struct ASTRoot {
    std::vector<std::unique_ptr<OuterNode>> nodes;
};

}; // namespace parser

#endif // AST_NODES_H
