#include "token.h"
#include <iostream>

static std::string toString(Token::Type type)
{
    switch (type) {
    case Token::Identifier:
        return "Identifier";
    case Token::Keyword:
        return "Keyword";
    case Token::Operator:
        return "Operator";
    case Token::Punctator:
        return "Punctator";
    case Token::NumericLiteral:
        return "NumericLiteral";
    case Token::StringLiteral:
        return "StringLiteral";
    case Token::CharLiteral:
        return "CharLiteral";
    case Token::Comment:
        return "Comment";
    case Token::Undefined:
        return "Undefined";
    default:
        return "";
    }
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
    if (!t.value().empty())
        os << ", \"" << t.value() << "\"";
    os << ">";
    return os;
}
