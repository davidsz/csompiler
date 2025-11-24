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
    std::vector<Declaration> Build();

    int ErrorCode();
    std::string ErrorMessage();

private:
    std::string Consume(TokenType type, std::string_view value = "");
    std::optional<lexer::Token> Peek(long n = 0);

    Expression ParseExpression(int min_precedence);
    Expression ParseFactor();
    Expression ParseFunctionCall();

    Statement ParseReturn();
    Statement ParseIf();
    Statement ParseGoto();
    Statement ParseLabeledStatement();
    Statement ParseBreak();
    Statement ParseContinue();
    Statement ParseWhile();
    Statement ParseDoWhile();
    Statement ParseFor();
    Statement ParseSwitch();
    Statement ParseCase();
    Statement ParseDefault();
    Statement ParseBlock();
    BlockItem ParseBlockItem();
    Statement ParseStatement();

    Declaration ParseFunctionDeclaration();
    Declaration ParseVariableDeclaration();
    Declaration ParseDeclaration();

    const std::list<lexer::Token> &m_tokens;
    std::list<lexer::Token>::const_iterator m_pos;
    Error m_error;
    std::string m_message;
};

}; // namespace parser
