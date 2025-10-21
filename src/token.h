#ifndef TOKEN_H
#define TOKEN_H

#include <string>

class Token
{
public:
    enum Type {
        Identifier,
        Keyword,
        Operator,
        Punctator,
        Symbol,
        NumericLiteral,
        StringLiteral,
        Undefined,
    };

    Token();
    Token(Type type, std::string_view value = "");
    ~Token() = default;

    Type type() const { return m_type; }
    std::string value() const { return m_value; }
    void setValue(const std::string &value) { m_value = value; }

    friend std::ostream &operator<<(std::ostream &os, const Token &t);

private:
    Type m_type;
    std::string m_value;
};

#endif // TOKEN_H
