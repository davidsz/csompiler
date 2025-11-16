#include "ast_builder.h"
#include "ast_nodes.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace parser {

struct SyntaxError : public std::runtime_error
{
    explicit SyntaxError(const std::string &message) : std::runtime_error(message) {}
};

ASTBuilder::ASTBuilder(const std::list<lexer::Token> &tokens)
    : m_tokens(tokens)
    , m_pos(tokens.begin())
    , m_error(ALL_OK)
    , m_message("")
{
}

int ASTBuilder::ErrorCode()
{
    return m_error;
}

std::string ASTBuilder::ErrorMessage()
{
    return m_message;
}

std::string ASTBuilder::Consume(TokenType e_type, std::string_view e_value)
{
    if (m_pos == m_tokens.end()) {
        Abort("ASTBuilder reached the end of tokens.");
        return "";
    }

    if (m_pos->type() != e_type) {
        Abort(std::format("Expected {}, but found {}",
            lexer::Token::ToString(e_type),
            lexer::Token::ToString(m_pos->type())
        ), m_pos->line());
        return "";
    }
    if (e_value.length() > 0 && m_pos->value() != e_value) {
        Abort(std::format("Expected {}, but {} found", e_value, m_pos->value()), m_pos->line());
        return "";
    }
    LOG("Consumed " << (m_pos)->value());
    return (m_pos++)->value();
}

std::optional<lexer::Token> ASTBuilder::Peek(long n)
{
    if (std::distance(m_pos, m_tokens.end()) <= n)
        return std::nullopt;
    return *std::next(m_pos, n);
}

Expression ASTBuilder::ParseExpression(int min_precedence)
{
    LOG("ParseExpression");
    Expression left = ParseFactor();
    auto next = Peek();

    // Postfix unary expressions (left-associative)
    UnaryOperator unop = toUnaryOperator(next->value());
    if (unop && canBePostfix(unop)) {
        while (unop && canBePostfix(unop)) {
            Consume(TokenType::Operator);
            left = wrap_expression(UnaryExpression{ unop, UE(left), true });
            next = Peek();
            unop = toUnaryOperator(next->value());
        }
        return left;
    }

    // Binary expressions
    BinaryOperator op = toBinaryOperator(next->value());
    int precedence = getPrecedence(op);
    while (op && precedence >= min_precedence) {
        Consume(TokenType::Operator);
        if (op == BinaryOperator::Assign) {
            // Assignment ('=') is right-associative
            auto right = ParseExpression(precedence);
            left = wrap_expression(AssignmentExpression{ UE(left), UE(right) });
        } else if (isCompoundAssignment(op)) {
            // Compound assignments are right-associative
            auto right = ParseExpression(precedence);
            // We convert them to binary expressions
            left = wrap_expression(BinaryExpression{ op, UE(left), UE(right) });
        } else if (op == BinaryOperator::Conditional) {
            // Parse the middle part ("? expression :")
            auto middle = ParseExpression(0);
            Consume(TokenType::Operator, ":");
            // Parse the right part
            auto right = ParseExpression(precedence);
            left = wrap_expression(ConditionalExpression{ UE(left), UE(middle), UE(right) });
        } else {
            // Other binary operators are left-associative
            auto right = ParseExpression(precedence + 1);
            left = wrap_expression(BinaryExpression{ op, UE(left), UE(right) });
        }
        next = Peek();
        op = toBinaryOperator(next->value());
        precedence = getPrecedence(op);
    }
    return left;
}

Expression ASTBuilder::ParseFactor()
{
    LOG("ParseFactor");
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == "(") {
        Consume(TokenType::Punctator, "(");
        auto expr = ParseExpression(0);
        Consume(TokenType::Punctator, ")");
        return expr;
    }

    if (next->type() == TokenType::NumericLiteral) {
        double value = std::stod(Consume(TokenType::NumericLiteral));
        return wrap_expression(NumberExpression{ value });
    }

    if (next->type() == TokenType::Identifier)
        return wrap_expression(VariableExpression{ Consume(TokenType::Identifier) });

    // Prefix unary expressions (right-associative)
    if (next->type() == TokenType::Operator && isUnaryOperator(next->value())) {
        UnaryOperator op = toUnaryOperator(Consume(TokenType::Operator));
        auto expr = ParseExpression(getPrecedence(op) + 1);
        return wrap_expression(UnaryExpression{ op, UE(expr), false });
    }
    assert(false);
    return std::monostate();
}

Statement ASTBuilder::ParseReturn()
{
    LOG("ParseReturn");
    Consume(TokenType::Keyword, "return");
    auto ret = ReturnStatement{};
    ret.expr = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ";");
    return wrap_statement(std::move(ret));
}

Statement ASTBuilder::ParseIf()
{
    LOG("ParseIf");
    Consume(TokenType::Keyword, "if");
    auto ret = IfStatement{};
    Consume(TokenType::Punctator, "(");
    ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ")");
    ret.trueBranch = unique_statement(ParseStatement());

    auto next = Peek();
    if (next->type() == TokenType::Keyword && next->value() == "else") {
        Consume(TokenType::Keyword, "else");
        ret.falseBranch = unique_statement(ParseStatement());
    }
    return ret;
}

std::vector<BlockItem> ASTBuilder::ParseBlock()
{
    LOG("ParseBlock");
    Consume(TokenType::Punctator, "{");
    std::vector<BlockItem> block;
    for (auto next = Peek(); next && next->value() != "}"; next = Peek())
        block.push_back(ParseBlockItem());
    Consume(TokenType::Punctator, "}");
    return block;
}

BlockItem ASTBuilder::ParseBlockItem()
{
    LOG("ParseBlockItem");
    auto next = Peek();
    // TODO
    if (next->value() == "int")
        return to_block_item(ParseDeclaration());
    else
        return to_block_item(ParseStatement());
}

Statement ASTBuilder::ParseFunction()
{
    LOG("ParseFunction");
    auto func = FuncDeclStatement{};
    Consume(TokenType::Keyword);
    func.name = Consume(TokenType::Identifier);
    Consume(TokenType::Punctator, "(");
    func.params.push_back(Consume(TokenType::Keyword, "void"));
    Consume(TokenType::Punctator, ")");
    func.body = ParseBlock();
    return wrap_statement(std::move(func));
}

Statement ASTBuilder::ParseStatement()
{
    LOG("ParseStatement");
    auto next = Peek();
    if (next->type() == TokenType::Keyword) {
        if (next->value() == "return")
            return Statement(ParseReturn());
        if (next->value() == "if")
            return Statement(ParseIf());
        // TODO
        if (next->value() == "int")
            return ParseFunction();

        Abort("Not implemented yet.", next->line());
    }

    if (next->type() == TokenType::Punctator && next->value() == ";") {
        Consume(TokenType::Punctator, ";");
        return Statement(NullStatement{});
    }

    auto expr = ParseExpression(0);
    Consume(TokenType::Punctator, ";");
    return Statement(ExpressionStatement{ UE(expr) });
}

Declaration ASTBuilder::ParseDeclaration()
{
    LOG("ParseDeclaration");
    auto decl = Declaration{};
    Consume(TokenType::Keyword, "int");
    decl.identifier = Consume(TokenType::Identifier);
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == ";") {
        Consume(TokenType::Punctator, ";");
        return decl;
    }
    Consume(TokenType::Operator, "=");
    decl.init = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ";");
    return decl;
}

void ASTBuilder::Abort(std::string_view message, size_t line)
{
    m_error = PARSER_ERROR;
    auto l = static_cast<unsigned long>(line);
    if (line)
        m_message = std::format("Syntax error at line {}: {}", l, message);
    else
        m_message = std::format("Syntax error: {}", message);
    throw SyntaxError(m_message);
}

std::vector<BlockItem> ASTBuilder::Build()
{
    std::vector<BlockItem> root;
    try {
        while (Peek()) {
            auto statement = ParseStatement();
            root.push_back(to_block_item(std::move(statement)));
        }
    } catch (const SyntaxError &e) {
        std::cerr << e.what() << std::endl;
    }

    if (m_pos != m_tokens.end()) {
        std::cerr << "Asserting on " << m_pos->value() << std::endl;
        assert(m_pos == m_tokens.end());
    }
    return root;
}

} // namespace parser
