#ifndef TOKEN_H
#define TOKEN_H

#include <string>

namespace lexer {

class Token
{
public:
    enum Type {
        Identifier,
        Keyword,
        Operator,
        Punctator,
        NumericLiteral,
        StringLiteral,
        CharLiteral,
        Undefined,
    };

    static std::string ToString(Type type);

    Token();
    Token(Type type, std::string_view value, size_t line, size_t col);
    ~Token() = default;

    Type type() const { return m_type; }
    size_t line() const { return m_line; }
    size_t col() const { return m_col; }
    std::string value() const { return m_value; }
    void setValue(const std::string &value) { m_value = value; }

    friend std::ostream &operator<<(std::ostream &os, const Token &t);

private:
    Type m_type;
    size_t m_line;
    size_t m_col;
    std::string m_value;
};

}; // namespace lexer

#endif // TOKEN_H
