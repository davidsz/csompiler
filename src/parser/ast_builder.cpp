#include "ast_builder.h"
#include "ast_nodes.h"
#include <cassert>
#include <iostream>
#include <ranges>
#include <stdexcept>

namespace parser {

struct SyntaxError : public std::runtime_error {
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
    // TODO: Maybe the lexer should omit the comments?
    while (m_pos->type() == TokenType::Comment)
        m_pos++;

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

std::unique_ptr<ReturnStatement> ASTBuilder::ParseReturn()
{
    auto ret = std::make_unique<ReturnStatement>();
    Consume(TokenType::Keyword, "return");
    double value = std::stod(Consume(TokenType::NumericLiteral));
    ret->expr = make_expression<NumberExpression>(value);
    Consume(TokenType::Punctator, ";");
    return ret;
}

std::unique_ptr<BlockStatement> ASTBuilder::ParseBlock()
{
    auto block = std::make_unique<BlockStatement>();
    auto ret = ParseReturn();
    block->statements.push_back(wrap_statement(std::move(ret)));
    return block;
}

std::unique_ptr<FunctionDeclaration> ASTBuilder::ParseFunction()
{
    auto func = std::make_unique<FunctionDeclaration>();

    Consume(TokenType::Keyword);
    func->name = Consume(TokenType::Identifier);
    Consume(TokenType::Punctator, "(");

    func->params.push_back(Consume(TokenType::Keyword, "void"));

    Consume(TokenType::Punctator, ")");
    Consume(TokenType::Punctator, "{");
    func->body = ParseBlock();
    Consume(TokenType::Punctator, "}");

    return func;
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

std::unique_ptr<ASTRoot> ASTBuilder::Build()
{
    std::unique_ptr<ASTRoot> root = std::make_unique<ASTRoot>();
    try {
        auto func = ParseFunction();
        auto node = std::make_unique<OuterNode>(std::move(*func));
        root->nodes.push_back(std::move(node));
    } catch (const SyntaxError &e) {}
    return root;
}

} // namespace parser
