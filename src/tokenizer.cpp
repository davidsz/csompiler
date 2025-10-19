#include "tokenizer.h"

bool Tokenizer::Finished()
{
    return m_pos >= m_string.length() - 1;
}

char Tokenizer::NextChar()
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

Token Tokenizer::NextToken()
{
    return Token(Token::StringLiteral);
}
