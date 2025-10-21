#include "tokenizer.h"
#include <cassert>
#include <cctype>
#include <format>

static bool is_operator(char c) {
    static const bool lookup[256] = {
        ['+'] = true, ['-'] = true, ['*'] = true, ['/'] = true,
        ['<'] = true, ['>'] = true, ['^'] = true, ['?'] = true,
        ['%'] = true, ['!'] = true, ['='] = true, ['~'] = true,
        ['|'] = true, ['&'] = true, [','] = true, ['.'] = true,
        [':'] = true,
    };
    return lookup[(unsigned char)c];
}

static bool is_punctator(char c) {
    static const bool lookup[256] = {
        ['('] = true, ['['] = true, ['{'] = true,
        [')'] = true, [']'] = true, ['}'] = true,
        [';'] = true,
    };
    return lookup[(unsigned char)c];
}

static bool is_whitespace(char c) {
    return std::isblank(c) || c == '\n';
}

Tokenizer::Tokenizer(std::string_view s)
    : m_string(s)
    , m_pos(0)
    , m_line(1)
    , m_col(1)
    , m_error(ALL_OK)
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
    m_col++;
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

void Tokenizer::AbortAtPosition(Error error, std::string_view message)
{
    m_error = error;
    m_message = std::format("{} (line: {}, column: {})", message, m_line, m_col);
}

Token Tokenizer::MakeNumericLiteral()
{
    std::string literal;
    literal.reserve(10);

    char next = PeekNextChar();
    assert(std::isdigit(next));

    // TODO: Handle different types
    // - Binary: [0b/0B][0-1]
    // - Octal: 0[0-7]
    // - Hexadecimal [0x/0X][0-9a-fA-F]

    do {
        Step();
        literal += next;
        next = PeekNextChar();
    } while (std::isdigit(next));

    // TODO: It should end with whitespace, separator or semicolon

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
            AbortAtPosition(LEXER_ERROR, "Unclosed string literal");
            break;
        }

        next = PeekNextChar();
        // TODO: If the previous char was '\', don't break
        if (next == '"') {
            Step();
            break;
        }
    } while (true);

    return Token(Token::StringLiteral, literal);
}

Token Tokenizer::MakeWhitespace()
{
    assert(is_whitespace(PeekNextChar()));
    char c;
    while (is_whitespace(PeekNextChar())) {
        c = Step();
        if (c == '\n') {
            m_line++;
            m_col = 1;
        }
    }
    return Token(Token::Whitespace, "");
}

std::optional<Token> Tokenizer::NextToken()
{
    while (IsRunning()) {
        char c = PeekNextChar();

        if (std::isdigit(c))
            return MakeNumericLiteral();

        if (c == '"')
            return MakeStringLiteral();

        if (is_whitespace(c))
            return MakeWhitespace();

        if (is_operator(c))
            return Token(Token::Operator, std::string(1, Step()));

        if (is_punctator(c))
            return Token(Token::Punctator, std::string(1, Step()));

        AbortAtPosition(LEXER_ERROR, std::format("Can't recognize the character '{}' yet.", c));
    }
    return std::nullopt;
}
