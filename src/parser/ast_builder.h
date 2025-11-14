#pragma once

#include "ast_nodes.h"
#include "common/error.h"
#include "lexer/token.h"
#include <list>

namespace parser {

class ASTBuilder
{
public:
    using TokenType = lexer::Token::Type;

    ASTBuilder(const std::list<lexer::Token> &token_list);
    ~ASTBuilder() = default;

    void Abort(std::string_view, size_t line = 0);
    std::vector<BlockItem> Build();

    int ErrorCode();
    std::string ErrorMessage();

private:
    std::string Consume(TokenType type, std::string_view value = "");
    std::optional<lexer::Token> Peek(long n = 0);

    Expression ParseExpression(int min_precedence);
    Expression ParseFactor();

    Statement ParseReturn();
    Statement ParseFunction();
    Statement ParseStatement();

    Declaration ParseDeclaration();

    std::vector<BlockItem> ParseBlock();
    BlockItem ParseBlockItem();


    const std::list<lexer::Token> &m_tokens;
    std::list<lexer::Token>::const_iterator m_pos;
    Error m_error;
    std::string m_message;
};

}; // namespace parser
