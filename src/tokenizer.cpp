#include "tokenizer.h"
#include <cassert>
#include <cctype>
#include <format>

Tokenizer::Tokenizer(std::string_view s)
    : m_string(s)
    , m_pos(0)
    , m_error(0)
    , m_message("")
{
}

bool Tokenizer::IsRunning()
{
    return !ReachedEOF() && m_error == 0;
}

bool Tokenizer::ReachedEOF()
{
    return m_pos >= m_string.length();
}

int Tokenizer::ErrorCode()
{
    return m_error;
}

std::string Tokenizer::ErrorMessage()
{
    return m_message;
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

std::optional<Token> Tokenizer::NextToken()
{
    while (IsRunning()) {
        char c = PeekNextChar();

        if (std::isdigit(c))
            return MakeNumericLiteral();

        if (c == '"')
            return MakeStringLiteral();

        if (std::isblank(c) || c == '\n') {
            Step();
            continue;
        }

        m_error = 1;
        m_message = std::format("Can't recognize the character '{}' yet.", c);
    }
    return std::nullopt;
}
