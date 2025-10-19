#include "tokenizer.h"
#include <cassert>
#include <cctype>

bool Tokenizer::ReachedEOF()
{
    return m_pos >= m_string.length();
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
        Step();
        literal += next;
        next = PeekNextChar();
    } while (std::isdigit(next));

    return Token(Token::NumericLiteral, literal);
}

Token Tokenizer::MakeStringLiteral()
{
    std::string literal;
    literal.reserve(10);

    // We won't include the trailing ""
    char next = Step();
    assert(next == '"');
    do {
        next = Step();
        literal += next;

        if (ReachedEOF()) {
            // TODO: Error: we haven't found an ending "
            break;
        }

        next = PeekNextChar();
        if (next == '"') {
            Step();
            break;
        }
    } while (true);

    return Token(Token::StringLiteral, literal);
}

Token Tokenizer::NextToken()
{
    while (!ReachedEOF()) {
        char c = PeekNextChar();

        if (std::isdigit(c))
            return MakeNumericLiteral();

        if (c == '"')
            return MakeStringLiteral();

        if (std::isblank(c)) {
            Step();
            continue;
        }

        // TODO: Error, we can't recognize the currect character
        Step();
    }

    return Token(Token::EndOfFile);
}
