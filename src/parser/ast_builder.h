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
    Expression ParseUnaryExpression();
    Expression ParsePostfixExpression();
    Expression ParsePrimaryExpression();
    Expression ParseFunctionCall();
    Expression ParseStringExpression();
    Expression ParseConstantExpression();
    uint64_t ParseArrayDimension();

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

    // Declarator: specifies type with an identifier
    struct IdentifierDeclarator;
    struct PointerDeclarator;
    struct ArrayDeclarator;
    struct FunctionDeclarator;
    struct DeclaratorParam;
    using Declarator = std::variant<
        IdentifierDeclarator,
        PointerDeclarator,
        ArrayDeclarator,
        FunctionDeclarator
    >;

    // Abstract declarator: specifies type without an identifier
    struct AbstractBaseDeclarator;
    struct AbstractPointerDeclarator;
    struct AbstractArrayDeclarator;
    using AbstractDeclarator = std::variant<
        AbstractBaseDeclarator,
        AbstractPointerDeclarator,
        AbstractArrayDeclarator
    >;

    Declaration ParseDeclaration(bool allow_function = true);
    Declarator ParseDeclarator();
    std::tuple<std::string, Type, std::vector<std::string>>
        ProcessDeclarator(const Declarator &, const Type &);
    AbstractDeclarator ParseAbstractDeclarator();
    Type ProcessAbstractDeclarator(const AbstractDeclarator &, const Type &);
    Initializer ParseInitializer();

    // Parse type names and storage classes
    std::pair<StorageClass, Type> ParseTypeSpecifierList();
    // Parse type names only
    Type ParseTypes();

    const std::list<lexer::Token> &m_tokens;
    std::list<lexer::Token>::const_iterator m_pos;
    Error m_error;
    std::string m_message;
};

}; // namespace parser
