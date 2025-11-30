#include "ast_builder.h"
#include "ast_nodes.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace parser {

struct SyntaxError : public std::runtime_error
{
    explicit SyntaxError(const std::string &message) : std::runtime_error(message) {}
};

static const std::unordered_map<std::string, TypeSpecifier> s_typeSpecifiers {
#define ADD_TYPE_TO_MAP(enumname, stringname) {stringname, enumname},
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_MAP)
#undef ADD_TYPE_TO_MAP
};

static const std::unordered_map<std::string, StorageClass> s_storageClasses {
#define ADD_TYPE_TO_MAP(enumname, stringname) {stringname, enumname},
    STORAGE_CLASS_LIST(ADD_TYPE_TO_MAP)
#undef ADD_TYPE_TO_MAP
};

static const std::unordered_set<std::string> s_specifiers {
#define ADD_TYPE_TO_SET(enumname, stringname) stringname,
    TYPE_SPECIFIER_LIST(ADD_TYPE_TO_SET)
    STORAGE_CLASS_LIST(ADD_TYPE_TO_SET)
#undef ADD_TYPE_TO_SET
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
            left = UnaryExpression{ unop, UE(left), true };
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
            left = AssignmentExpression{ UE(left), UE(right) };
        } else if (isCompoundAssignment(op)) {
            // Compound assignments are right-associative
            auto right = ParseExpression(precedence);
            // We convert them to binary expressions
            left = BinaryExpression{ op, UE(left), UE(right) };
        } else if (op == BinaryOperator::Conditional) {
            // The middle part ("? expression :")
            // behaves like the operator of a binary expression
            auto middle = ParseExpression(0);
            Consume(TokenType::Operator, ":");
            auto right = ParseExpression(precedence); // Right-associative
            left = ConditionalExpression{ UE(left), UE(middle), UE(right) };
        } else {
            // Other binary operators are left-associative
            auto right = ParseExpression(precedence + 1);
            left = BinaryExpression{ op, UE(left), UE(right) };
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

    if (next->type() == TokenType::NumericLiteral)
        return NumberExpression{ std::stoi(Consume(TokenType::NumericLiteral)) };

    if (next->type() == TokenType::Identifier) {
        next = Peek(1);
        if (next->type() == TokenType::Punctator && next->value() == "(")
            return ParseFunctionCall();
        return VariableExpression{ Consume(TokenType::Identifier) };
    }

    // Prefix unary expressions (right-associative)
    if (next->type() == TokenType::Operator && isUnaryOperator(next->value())) {
        UnaryOperator op = toUnaryOperator(Consume(TokenType::Operator));
        auto expr = ParseExpression(getPrecedence(op) + 1);
        return UnaryExpression{ op, UE(expr), false };
    }
    assert(false);
    return std::monostate();
}

Expression ASTBuilder::ParseFunctionCall()
{
    auto ret = FunctionCallExpression{};
    ret.identifier = Consume(TokenType::Identifier);

    Consume(TokenType::Punctator, "(");
    auto next = Peek();
    while (next->value() != ")") {
        if (next->value() == ",")
            Consume(TokenType::Operator, ",");
        ret.args.push_back(unique_expression(ParseExpression(0)));
        next = Peek();
    }
    Consume(TokenType::Punctator, ")");
    return ret;
}

Statement ASTBuilder::ParseReturn()
{
    LOG("ParseReturn");
    Consume(TokenType::Keyword, "return");
    auto ret = ReturnStatement{};
    ret.expr = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ";");
    return ret;
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

Statement ASTBuilder::ParseGoto()
{
    LOG("ParseGoto");
    Consume(TokenType::Keyword, "goto");
    auto ret = GotoStatement{ Consume(TokenType::Identifier) };
    Consume(TokenType::Punctator, ";");
    return ret;
}

Statement ASTBuilder::ParseLabeledStatement()
{
    // In C17, labels are allowed only before statements and they
    // make a labeled statement together.
    LOG("ParseLabeledStatement");
    std::string label = Consume(TokenType::Identifier);
    Consume(TokenType::Operator, ":");
    return LabeledStatement{
        label,
        unique_statement(ParseStatement())
    };
}

Statement ASTBuilder::ParseBreak()
{
    LOG("ParseBreak");
    Consume(TokenType::Keyword, "break");
    Consume(TokenType::Punctator, ";");
    return BreakStatement{};
}

Statement ASTBuilder::ParseContinue()
{
    LOG("ParseContinue");
    Consume(TokenType::Keyword, "continue");
    Consume(TokenType::Punctator, ";");
    return ContinueStatement{};
}

Statement ASTBuilder::ParseWhile()
{
    LOG("ParseWhile");
    auto ret = WhileStatement{};
    Consume(TokenType::Keyword, "while");
    Consume(TokenType::Punctator, "(");
    ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ")");
    ret.body = unique_statement(ParseStatement());
    return ret;
}

Statement ASTBuilder::ParseDoWhile()
{
    LOG("ParseDoWhile");
    auto ret = DoWhileStatement{};
    Consume(TokenType::Keyword, "do");
    ret.body = unique_statement(ParseStatement());
    Consume(TokenType::Keyword, "while");
    Consume(TokenType::Punctator, "(");
    ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ")");
    Consume(TokenType::Punctator, ";");
    return ret;
}

Statement ASTBuilder::ParseFor()
{
    LOG("ParseFor");
    auto ret = ForStatement{};
    Consume(TokenType::Keyword, "for");
    Consume(TokenType::Punctator, "(");

    // Initializer
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == ";") {
        Consume(TokenType::Punctator, ";");
    } else if (next->type() == TokenType::Keyword) {
        ret.init = std::make_unique<ForInit>(
            to_for_init(ParseDeclaration(/* allow_function */ false)));
    } else {
        ret.init = std::make_unique<ForInit>(
            to_for_init(ParseExpression(0)));
        Consume(TokenType::Punctator, ";");
    }

    // Condition
    next = Peek();
    if (next->type() != TokenType::Punctator)
        ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ";");

    // Update
    next = Peek();
    if (next->type() != TokenType::Punctator)
        ret.update = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ")");

    // Body
    ret.body = unique_statement(ParseStatement());
    return ret;
}

Statement ASTBuilder::ParseSwitch()
{
    LOG("ParseSwitch");
    auto ret = SwitchStatement{};
    Consume(TokenType::Keyword, "switch");
    Consume(TokenType::Punctator, "(");
    ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Punctator, ")");
    ret.body = unique_statement(ParseStatement());
    return ret;
}

Statement ASTBuilder::ParseCase()
{
    LOG("ParseCase");
    auto ret = CaseStatement{};
    Consume(TokenType::Keyword, "case");
    ret.condition = unique_expression(ParseExpression(0));
    Consume(TokenType::Operator, ":");
    ret.statement = unique_statement(ParseStatement());
    return ret;
}

Statement ASTBuilder::ParseDefault()
{
    LOG("ParseDefault");
    Consume(TokenType::Keyword, "default");
    Consume(TokenType::Operator, ":");
    return DefaultStatement{
        unique_statement(ParseStatement()),
        ""
    };
}

Statement ASTBuilder::ParseBlock()
{
    LOG("ParseBlock");
    Consume(TokenType::Punctator, "{");
    auto block = BlockStatement{};
    for (auto next = Peek(); next && next->value() != "}"; next = Peek())
        block.items.push_back(ParseBlockItem());
    Consume(TokenType::Punctator, "}");
    return block;
}

BlockItem ASTBuilder::ParseBlockItem()
{
    LOG("ParseBlockItem");
    auto next = Peek();
    if (s_specifiers.contains(next->value()))
        return to_block_item(ParseDeclaration());
    else
        return to_block_item(ParseStatement());
}

Statement ASTBuilder::ParseStatement()
{
    LOG("ParseStatement");
    auto next = Peek();
    if (next->type() == TokenType::Keyword) {
        if (next->value() == "return")
            return ParseReturn();
        if (next->value() == "if")
            return ParseIf();
        if (next->value() == "goto")
            return ParseGoto();
        if (next->value() == "break")
            return ParseBreak();
        if (next->value() == "continue")
            return ParseContinue();
        if (next->value() == "while")
            return ParseWhile();
        if (next->value() == "do")
            return ParseDoWhile();
        if (next->value() == "for")
            return ParseFor();
        if (next->value() == "switch")
            return ParseSwitch();
        if (next->value() == "case")
            return ParseCase();
        if (next->value() == "default")
            return ParseDefault();
        Abort("Not implemented yet.", next->line());
    }

    if (next->type() == TokenType::Punctator) {
        if (next->value() == "{")
            return ParseBlock();
        if (next->value() == ";") {
            Consume(TokenType::Punctator, ";");
            return Statement(NullStatement{});
        }
    }

    if (next->type() == TokenType::Identifier) {
        next = Peek(1);
        if (next->type() == TokenType::Operator && next->value() == ":")
            return ParseLabeledStatement();
    }

    auto expr = ParseExpression(0);
    Consume(TokenType::Punctator, ";");
    return Statement(ExpressionStatement{ UE(expr) });
}

Declaration ASTBuilder::ParseDeclaration(bool allow_function)
{
    LOG("ParseDeclaration");
    std::vector<TypeSpecifier> type_specifiers;
    std::vector<StorageClass> storage_classes;
    std::optional<lexer::Token> next;
    for (next = Peek(); next->type() == TokenType::Keyword; next = Peek()) {
        auto type_it = s_typeSpecifiers.find(next->value());
        if (type_it != s_typeSpecifiers.end()) {
            type_specifiers.push_back(type_it->second);
            Consume(TokenType::Keyword);
            continue;
        }

        auto storage_it = s_storageClasses.find(next->value());
        if (storage_it != s_storageClasses.end()) {
            storage_classes.push_back(storage_it->second);
            Consume(TokenType::Keyword);
        }
    }

    if (type_specifiers.size() != 1)
        Abort("Invalid type specification");
    if (storage_classes.size() > 1)
        Abort("Invalid storage class");

    StorageClass storage = storage_classes.empty() ? StorageClass::StorageDefault : storage_classes[0];
    std::string identifier = Consume(TokenType::Identifier);

    next = Peek();
    if (allow_function && next->type() == TokenType::Punctator && next->value() == "(") {
        // Function declaration
        auto func = FunctionDeclaration{};
        func.storage = storage;
        func.name = std::move(identifier);
        Consume(TokenType::Punctator, "(");
        next = Peek();
        // 'void' must be there in empty parameter lists
        if (next->type() == TokenType::Keyword && next->value() == "void")
            Consume(TokenType::Keyword);
        else {
            while (next->value() != ")") {
                if (next->value() == ",")
                    Consume(TokenType::Operator, ",");
                Consume(TokenType::Keyword);
                func.params.push_back(Consume(TokenType::Identifier));
                next = Peek();
            }
        }
        Consume(TokenType::Punctator, ")");

        next = Peek();
        if (next->type() == TokenType::Punctator && next->value() == "{")
            func.body = unique_statement(ParseBlock());
        else
            Consume(TokenType::Punctator, ";");
        return func;
    } else {
        // Variable declaration
        auto decl = VariableDeclaration{};
        decl.storage = storage;
        decl.identifier = std::move(identifier);
        if (next->type() == TokenType::Punctator && next->value() == ";") {
            Consume(TokenType::Punctator, ";");
            return decl;
        }
        Consume(TokenType::Operator, "=");
        decl.init = unique_expression(ParseExpression(0));
        Consume(TokenType::Punctator, ";");
        return decl;
    }
    assert(false);
    return VariableDeclaration{};
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

std::vector<Declaration> ASTBuilder::Build()
{
    std::vector<Declaration> root;
    try {
        while (Peek())
            root.push_back(ParseDeclaration());

        if (m_pos != m_tokens.end()) {
            std::cerr << "Asserting on " << m_pos->value() << std::endl;
            assert(m_pos == m_tokens.end());
        }
    } catch (const SyntaxError &e) {
        std::cerr << e.what() << std::endl;
    }
    return root;
}

} // namespace parser
