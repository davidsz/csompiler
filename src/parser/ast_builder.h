#ifndef AST_BUILDER_H
#define AST_BUILDER_H

#include "ast_nodes.h"
#include "error.h"
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
    std::unique_ptr<ASTRoot> Build();

    int ErrorCode();
    std::string ErrorMessage();

private:
    std::string Consume(TokenType type, std::string_view value = "");

    std::unique_ptr<ReturnStatement> ParseReturn();
    std::unique_ptr<BlockStatement> ParseBlock();
    std::unique_ptr<FunctionDeclaration> ParseFunction();

    const std::list<lexer::Token> &m_tokens;
    std::list<lexer::Token>::const_iterator m_pos;
    Error m_error;
    std::string m_message;
};

}; // namespace parser

#endif // AST_BUILDER_H
