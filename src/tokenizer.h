#ifndef TOKENIZER_H
#define TOKENIZER_H

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

    Token MakeNumericLiteral();
    Token MakeStringLiteral();

    std::string_view m_string;
    size_t m_pos;
    int m_error;
    std::string m_message;
};

#endif // TOKENIZER_H
