#pragma once

#include "common/error.h"
#include "token.h"
#include <optional>
#include <string_view>

namespace lexer {

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
    char PeekNextChar(size_t i = 0);
    char PreviousChar();

    void AbortAtPosition(std::string_view);
    Token CreateToken(Token::Type type, std::string_view content);

    void SkipWhitespace();
    void SkipComment();
    Token MakeNumericLiteral();
    std::string ParseNumericSuffixes(bool is_fractional, bool after_exponent);
    std::string ParseExponent();
    Token MakeStringLiteral();
    Token MakeCharLiteral();
    Token MakeIdentifierOrKeyword();
    Token MakeOperator(char first);

    std::string_view m_string;
    size_t m_pos;
    size_t m_line;
    size_t m_col;
    Error m_error;
    std::string m_message;
};

}; // namespace lexer
