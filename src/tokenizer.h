#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "token.h"
#include <string_view>

class Tokenizer
{
public:
    Tokenizer(std::string_view s) : m_string(s) {}
    ~Tokenizer() = default;

    bool Finished();
    bool ReachedEOF();
    Token NextToken();

private:
    char Step();
    char PeekNextChar();
    char PreviousChar();

    Token MakeNumericLiteral();
    Token MakeStringLiteral();

    std::string_view m_string;
    size_t m_pos = 0;
};

#endif // TOKENIZER_H
