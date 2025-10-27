#include "token.h"
#include <iostream>

namespace lexer {

static std::string toString(Token::Type type)
{
    switch (type) {
    case Token::Identifier:
        return "identifier";
    case Token::Keyword:
        return "keyword";
    case Token::Operator:
        return "operator";
    case Token::Punctator:
        return "punctator";
    case Token::NumericLiteral:
        return "numeric literal";
    case Token::StringLiteral:
        return "string literal";
    case Token::CharLiteral:
        return "char literal";
    case Token::Comment:
        return "comment";
    case Token::Undefined:
        return "undefined";
    default:
        return "";
    }
}

std::string Token::ToString(Type type)
{
    return toString(type);
}

Token::Token()
    : m_type(Type::Undefined)
    , m_line(0)
    , m_col(0)
    , m_value("")
{
}

Token::Token(Type type, std::string_view value, size_t line, size_t col)
    : m_type(type)
    , m_line(line)
    , m_col(col)
    , m_value(value)
{
}

std::ostream &operator<<(std::ostream &os, const Token &t)
{
    os << "<" << toString(t.type());
    os << " (" << t.line() << ", " << t.col() << ")";
    if (!t.value().empty())
        os << ", \"" << t.value() << "\"";
    os << ">";
    return os;
}

}; // namespace lexer
