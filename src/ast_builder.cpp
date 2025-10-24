#include "ast_builder.h"
#include "ast_nodes.h"
#include <cassert>
#include <iostream>
#include <ranges>

namespace parser {

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
        std::cout << "The ASTBuilder reached the end of tokens." << std::endl;
        return "";
    }
    if (m_pos->type() != e_type) {
        Abort(std::format("Error: Expected {} at line {}, but found {}.",
            lexer::Token::ToString(e_type),
            static_cast<unsigned long>(m_pos->line()),
            lexer::Token::ToString(m_pos->type())
        ));
        return "";
    }
    if (e_value.length() > 0 && m_pos->value() != e_value) {
        Abort(std::format("Error: Expected {} at line {}, but found {}.",
            e_value,
            static_cast<unsigned long>(m_pos->line()),
            m_pos->value()
        ));
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
    Consume(TokenType::Comment);
    ParseReturn();
    return std::make_unique<BlockStatement>();
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

void ASTBuilder::Abort(std::string_view message)
{
    m_error = PARSER_ERROR;
    m_message = message;
}

std::unique_ptr<ASTRoot> ASTBuilder::Build()
{
    std::unique_ptr<ASTRoot> root = std::make_unique<ASTRoot>();

    auto func = ParseFunction();
    auto node = std::make_unique<OuterNode>(std::move(*func));
    root->nodes.push_back(std::move(node));

    return root;
}

} // namespace parser
