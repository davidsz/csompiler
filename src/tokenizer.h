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

    char NextChar();
    char PeekNextChar();
    char PreviousChar();

    Token NextToken();

private:
    std::string_view m_string;
    size_t m_pos = 0;
};

#endif // TOKENIZER_H
