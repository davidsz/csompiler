#include "tokenizer.h"
#include <cassert>
#include <cctype>
#include <format>
#include <unordered_set>

static bool is_keyword(std::string_view word)
{
    static const std::unordered_set<std::string> keywords = {
        "alignas", "alignof", "auto", "bool", "break", "case", "char", "const",
        "constexpr", "continue", "default", "do", "double", "else", "enum", "extern",
        "false", "float", "for", "goto", "if", "inline", "int", "long", "nullptr",
        "register", "restrict", "return", "short", "signed", "sizeof", "static",
        "static_assert", "struct", "switch", "thread_local", "true", "typedef", "typeof",
        "typeof_unqual", "union", "unsigned", "void", "volatile", "while", "_Alignas",
        "_Alignof", "_Atomic", "_BitInt", "_Bool", "_Complex", "_Decimal128", "_Decimal32",
        "_Decimal64", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
    };
    return keywords.contains(std::string(word));
}

static bool is_operator(char c)
{
    static const bool lookup[256] = {
        ['+'] = true, ['-'] = true, ['*'] = true, ['/'] = true,
        ['<'] = true, ['>'] = true, ['^'] = true, ['?'] = true,
        ['%'] = true, ['!'] = true, ['='] = true, ['~'] = true,
        ['|'] = true, ['&'] = true, [','] = true, ['.'] = true,
        [':'] = true,
    };
    return lookup[(unsigned char)c];
}

static bool is_punctator(char c)
{
    static const bool lookup[256] = {
        ['('] = true, ['['] = true, ['{'] = true,
        [')'] = true, [']'] = true, ['}'] = true,
        [';'] = true,
    };
    return lookup[(unsigned char)c];
}

static bool is_whitespace(char c)
{
    return std::isblank(c) || c == '\n';
}

namespace lexer {

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
    char ret = m_string[m_pos++];
    if (ret == '\n') {
        m_line++;
        m_col = 1;
    } else
        m_col++;
    return ret;
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

void Tokenizer::AbortAtPosition(std::string_view message)
{
    m_error = LEXER_ERROR;
    m_message = std::format("{} (line: {}, column: {})", message, m_line, m_col);
}

Token Tokenizer::CreateToken(Token::Type type, std::string_view content)
{
    return Token(type, content, m_line, m_col);
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

    size_t l_count = 0;
    size_t u_count = 0;
    do {
        Step();
        literal += next;
        next = PeekNextChar();
        if (l_count == 0 && u_count == 0 && std::isdigit(next))
            continue;
        if (next == 'l' || next == 'L') {
            if (++l_count > 1)
                AbortAtPosition("Numeric literals can have only one L suffix.");
            continue;
        }
        if (next == 'u' || next == 'U') {
            if (++u_count > 1)
                AbortAtPosition("Numeric literals can have only one U suffix.");
            continue;
        }
        break;
    } while (true);

    // We started to process an identifier starting with a number, it's invalid.
    if (!is_whitespace(next) && !is_operator(next) && !is_punctator(next))
        AbortAtPosition("Identifiers can't start with numbers.");

    return CreateToken(Token::NumericLiteral, literal);
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
        if (next == '\\')
            next = Step();
        literal += next;

        if (ReachedEOF()) {
            AbortAtPosition("Unclosed string literal");
            break;
        }

        next = PeekNextChar();
        if (next == '"') {
            Step();
            break;
        }
    } while (true);

    return CreateToken(Token::StringLiteral, literal);
}

Token Tokenizer::MakeCharLiteral()
{
    if (m_pos > m_string.length() - 3) {
        AbortAtPosition("Invalid char literal");
        return Token();
    }

    // We won't include the trailing ''
    char next = Step();
    assert(next == '\'');

    char character = 0;
    next = Step();
    if (next == '\\')
        next = Step();
    character = next;

    next = Step();
    if (next != '\'')
        AbortAtPosition("Invalid char literal");
    return CreateToken(Token::CharLiteral, std::string(1, character));
}

void Tokenizer::SkipWhitespace()
{
    assert(is_whitespace(PeekNextChar()));
    while (is_whitespace(PeekNextChar()))
        Step();
}

Token Tokenizer::MakeIdentifierOrKeyword()
{
    std::string word;
    word.reserve(10);

    char next = PeekNextChar();
    // They can't start with numbers
    assert(next == '_' || std::isalpha(next));
    do {
        Step();
        word += next;
        next = PeekNextChar();
    } while (next == '_' || std::isalnum(next));

    return CreateToken(is_keyword(word) ? Token::Keyword :  Token::Identifier, word);
}

Token Tokenizer::MakeOperator(char first)
{
    char next = PeekNextChar();
    switch (first) {
        case '-':
            if (next == '-') { Step(); return CreateToken(Token::Operator, "--"); }
            if (next == '=') { Step(); return CreateToken(Token::Operator, "-="); }
            break;
        case '+':
            if (next == '+') { Step(); return CreateToken(Token::Operator, "++"); }
            if (next == '=') { Step(); return CreateToken(Token::Operator, "+="); }
            break;
        case '*':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "*="); }
            break;
        case '/':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "/="); }
            break;
        case '%':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "%="); }
            break;
        case '<':
            if (next == '<') {
                Step(); next = PeekNextChar();
                if (next == '=') { Step(); return CreateToken(Token::Operator, "<<="); }
                return CreateToken(Token::Operator, "<<");
            }
            if (next == '=') { Step(); return CreateToken(Token::Operator, "<="); }
            break;
        case '>':
            if (next == '>') {
                Step(); next = PeekNextChar();
                if (next == '=') { Step(); return CreateToken(Token::Operator, ">>="); }
                return CreateToken(Token::Operator, ">>");
            }
            if (next == '=') { Step(); return CreateToken(Token::Operator, ">="); }
            break;
        case '&':
            if (next == '&') { Step(); return CreateToken(Token::Operator, "&&"); }
            if (next == '=') { Step(); return CreateToken(Token::Operator, "&="); }
            break;
        case '|':
            if (next == '|') { Step(); return CreateToken(Token::Operator, "||"); }
            if (next == '=') { Step(); return CreateToken(Token::Operator, "|="); }
            break;
        case '=':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "=="); }
            break;
        case '!':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "!="); }
            break;
        case '^':
            if (next == '=') { Step(); return CreateToken(Token::Operator, "^="); }
            break;
    }
    return CreateToken(Token::Operator, std::string(1, first));
}

void Tokenizer::SkipComment()
{
    bool oneliner = true;
    char next = Step();
    // We already jumped over the initial /
    assert(next == '/' || next == '*');
    oneliner = (next == '/');

    do {
        next = PeekNextChar();
        if (oneliner && next == '\n') {
            Step();
            break;
        }
        if (!oneliner && next == '*') {
            Step();
            if (PeekNextChar() == '/') {
                Step();
                break;
            }
        }
        Step();
        if (!oneliner && ReachedEOF())
            AbortAtPosition("Unclosed comment block");
    } while (IsRunning());
}

std::optional<Token> Tokenizer::NextToken()
{
    while (IsRunning()) {
        char c = PeekNextChar();

        if (is_whitespace(c)) {
            SkipWhitespace();
            continue;
        }

        if (std::isdigit(c))
            return MakeNumericLiteral();

        if (c == '"')
            return MakeStringLiteral();

        if (c == '\'')
            return MakeCharLiteral();

        if (c == '_' || std::isalpha(c))
            return MakeIdentifierOrKeyword();

        if (is_operator(c)) {
            char op = Step();
            if (op == '/') {
                c = PeekNextChar();
                if (c == '/' || c == '*') {
                    SkipComment();
                    continue;
                }
            }
            return MakeOperator(op);
        }

        if (is_punctator(c))
            return CreateToken(Token::Punctator, std::string(1, Step()));

        AbortAtPosition(std::format("Can't recognize the character '{}'.", c));
    }
    return std::nullopt;
}

}; // namespace lexer
