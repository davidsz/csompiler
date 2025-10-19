#include "tokenizer.h"
#include <cassert>
#include <cctype>

bool Tokenizer::Finished()
{
    return m_pos >= m_string.length() - 1;
}

char Tokenizer::Step()
{
    if (m_string.length() == m_pos - 1)
        return 0;
    return m_string[m_pos++];
}

char Tokenizer::PeekNextChar()
{
    if (m_string.length() == m_pos - 1)
        return 0;
    return m_string[m_pos];
}

char Tokenizer::PreviousChar()
{
    if (m_pos == 0)
        return 0;
    return m_string[m_pos - 1];
}

Token Tokenizer::MakeNumericLiteral()
{
    std::string literal;
    literal.reserve(10);

    char next = PeekNextChar();
    assert(std::isdigit(next));
    do {
        next = Step();
        literal += next;
        next = PeekNextChar();
    } while (std::isdigit(next));

    return Token(Token::NumericLiteral, literal);
}

Token Tokenizer::NextToken()
{
    // Runs until we can't construct a token
    while (m_pos < m_string.length()) {
        char c = PeekNextChar();

        if (std::isdigit(c))
            return MakeNumericLiteral();

        if (std::isblank(c)) {
            Step();
            continue;
        }

        Step();
    }

    return Token(Token::StringLiteral);
}
