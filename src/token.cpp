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
    case Token::Symbol:
        return "Symbol";
    case Token::NumericLiteral:
        return "NumericLiteral";
    case Token::StringLiteral:
        return "StringLiteral";
    case Token::EndOfFile:
        return "EndOfFile";
    case Token::Undefined:
        return "Undefined";
    default:
        return "";
    }
}

Token::Token()
    : m_type(Type::Undefined)
    , m_value("")
{
}

Token::Token(Type type, std::string value)
    : m_type(type)
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
