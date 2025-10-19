#ifndef TOKEN_H
#define TOKEN_H

class Token
{
public:
    enum Type {
        Identifier,
        Keyword,
        Operator,
        Symbol,
        NumericLiteral,
        StringLiteral,
    };

    Token(Type type)
        : m_type(type) {}
    ~Token() = default;

    Type type() { return m_type; }

    // TODO:
    // value
    // pretty print

private:
    Type m_type;
};

#endif // TOKEN_H
