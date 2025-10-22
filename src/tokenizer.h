#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "error.h"
#include "token.h"
#include <optional>
#include <string_view>

class Tokenizer
{
public:
    Tokenizer(std::string_view s);
    ~Tokenizer() = default;

    bool IsRunning();
    bool ReachedEOF();
    std::optional<Token> NextToken();
    int ErrorCode();
    std::string ErrorMessage();

private:
    char Step();
    char PeekNextChar();
    char PreviousChar();

    void AbortAtPosition(std::string_view);
    Token CreateToken(Token::Type type, std::string_view content);

    void SkipWhitespace();
    Token MakeNumericLiteral();
    Token MakeStringLiteral();
    Token MakeCharLiteral();
    Token MakeIdentifierOrKeyword();
    Token MakeComment();

    std::string_view m_string;
    size_t m_pos;
    size_t m_line;
    size_t m_col;
    Error m_error;
    std::string m_message;
};

#endif // TOKENIZER_H
