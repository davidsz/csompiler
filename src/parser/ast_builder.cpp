#include "ast_builder.h"
#include "ast_nodes.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace parser {

struct SyntaxError : public std::runtime_error
{
    explicit SyntaxError(const std::string &message) : std::runtime_error(message) {}
};

ASTBuilder::ASTBuilder(const std::list<lexer::Token> &tokens)
    : m_tokens(tokens)
    , m_pos(tokens.begin())
    , m_error(ALL_OK)
    , m_message("")
{
}

int ASTBuilder::ErrorCode()
{
    return m_error;
}

std::string ASTBuilder::ErrorMessage()
{
    return m_message;
}

std::string ASTBuilder::Consume(TokenType e_type, std::string_view e_value)
{
    if (m_pos == m_tokens.end()) {
        Abort("ASTBuilder reached the end of tokens.");
        return "";
    }

    if (m_pos->type() != e_type) {
        Abort(std::format("Expected {}, but found {}",
            lexer::Token::ToString(e_type),
            lexer::Token::ToString(m_pos->type())
        ), m_pos->line());
        return "";
    }
    if (e_value.length() > 0 && m_pos->value() != e_value) {
        Abort(std::format("Expected {}, but {} found", e_value, m_pos->value()), m_pos->line());
        return "";
    }
    return (m_pos++)->value();
}

std::optional<lexer::Token> ASTBuilder::Peek(long n)
{
    if (std::distance(m_pos, m_tokens.end()) <= n)
        return std::nullopt;
    return *std::next(m_pos, n);
}

std::unique_ptr<Expression> ASTBuilder::ParseExpression(int min_precedence)
{
    auto left = ParseFactor();
    auto next = Peek();
    BinaryOperator op = toBinaryOperator(next->value());
    int precedence = getPrecedence(op);
    while (op && precedence >= min_precedence) {
        Consume(TokenType::Operator);
        auto right = ParseExpression(precedence + 1);
        left = make_expression<BinaryExpression>(op, std::move(left), std::move(right));
        next = Peek();
        op = toBinaryOperator(next->value());
        precedence = getPrecedence(op);
    }
    return left;
}

std::unique_ptr<Expression> ASTBuilder::ParseFactor()
{
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == "(") {
        Consume(TokenType::Punctator, "(");
        auto expr = ParseExpression(0);
        Consume(TokenType::Punctator, ")");
        return expr;
    }
    if (next->type() == TokenType::NumericLiteral) {
        double value = std::stod(Consume(TokenType::NumericLiteral));
        return make_expression<NumberExpression>(value);
    }
    if (next->type() == TokenType::Operator && isUnaryOperator(next->value())) {
        UnaryOperator op = toUnaryOperator(Consume(TokenType::Operator));
        auto expr = ParseExpression(0);
        return make_expression<UnaryExpression>(op, std::move(expr));
    }
    assert(false);
}

std::unique_ptr<Statement> ASTBuilder::ParseReturn()
{
    auto ret = std::make_unique<ReturnStatement>();
    Consume(TokenType::Keyword, "return");
    ret->expr = ParseExpression(0);
    Consume(TokenType::Punctator, ";");
    return wrap_statement(std::move(ret));
}

std::unique_ptr<Statement> ASTBuilder::ParseBlock()
{
    auto block = std::make_unique<BlockStatement>();
    auto ret = ParseReturn();
    block->statements.push_back(wrap_statement(std::move(ret)));
    return wrap_statement(std::move(block));
}

std::unique_ptr<Statement> ASTBuilder::ParseFunction()
{
    auto func = std::make_unique<FuncDeclStatement>();

    Consume(TokenType::Keyword);
    func->name = Consume(TokenType::Identifier);
    Consume(TokenType::Punctator, "(");

    func->params.push_back(Consume(TokenType::Keyword, "void"));

    Consume(TokenType::Punctator, ")");
    Consume(TokenType::Punctator, "{");
    func->body = ParseBlock();
    Consume(TokenType::Punctator, "}");

    return wrap_statement(std::move(func));
}

std::unique_ptr<Statement> ASTBuilder::ParseStatement()
{
    auto next = Peek();
    if (next->type() == TokenType::Keyword) {
        if (next->value() == "return")
            return ParseReturn();
        // TODO
        if (next->value() == "int")
            return ParseFunction();

        Abort("Not implemented yet.", next->line());
    }

    if (next->type() == TokenType::Identifier) {
        Abort("Not implemented yet.", next->line());
    }

    return nullptr;
}

void ASTBuilder::Abort(std::string_view message, size_t line)
{
    m_error = PARSER_ERROR;
    auto l = static_cast<unsigned long>(line);
    if (line)
        m_message = std::format("Syntax error at line {}: {}", l, message);
    else
        m_message = std::format("Syntax error: {}", message);
    throw SyntaxError(m_message);
}

std::unique_ptr<BlockStatement> ASTBuilder::Build()
{
    std::unique_ptr<BlockStatement> root = std::make_unique<BlockStatement>();
    try {
        while (Peek())
            root->statements.push_back(ParseStatement());
    } catch (const SyntaxError &e) {
        // Setting the error code and message were handled already.
    }
    assert(m_pos == m_tokens.end());
    return root;
}

} // namespace parser
