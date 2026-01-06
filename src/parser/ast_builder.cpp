#include "ast_builder.h"
#include "ast_nodes.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace parser {

struct SyntaxError : public std::runtime_error
{
    explicit SyntaxError(const std::string &message) : std::runtime_error(message) {}
};

struct ASTBuilder::DeclaratorParam {
    Type type;
    std::unique_ptr<Declarator> declarator;
};
struct ASTBuilder::IdentifierDeclarator {
    std::string identifier;
};
struct ASTBuilder::PointerDeclarator {
    std::unique_ptr<Declarator> inner_declarator;
};
struct ASTBuilder::ArrayDeclarator {
    uint64_t size;
    std::unique_ptr<Declarator> inner_declarator;
};
struct ASTBuilder::FunctionDeclarator {
    std::vector<DeclaratorParam> params;
    std::unique_ptr<Declarator> inner_declarator;
};

struct ASTBuilder::AbstractBaseDeclarator {};
struct ASTBuilder::AbstractPointerDeclarator {
    std::unique_ptr<AbstractDeclarator> inner_declarator;
};
struct ASTBuilder::AbstractArrayDeclarator {
    uint64_t size;
    std::unique_ptr<AbstractDeclarator> inner_declarator;
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
    Expression left = ParseUnaryExpression();
    auto next = Peek();

    // Postfix unary expressions (left-associative)
    UnaryOperator unop = toUnaryOperator(next->value());
    while (unop && canBePostfix(unop)) {
        Consume(TokenType::Operator);
        left = UnaryExpression{ unop, UE(left), true };
        next = Peek();
        unop = toUnaryOperator(next->value());
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
            auto right = ParseExpression(precedence); // Right-associative
            left = CompoundAssignmentExpression{ op, UE(left), UE(right) };
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

Expression ASTBuilder::ParseUnaryExpression()
{
    LOG("ParseUnaryExpression");
    auto next = Peek();

    // Prefix unary expressions (right-associative)
    if (next->type() == TokenType::Operator && isUnaryOperator(next->value())) {
        LOG("Prefix Unary Expression");
        UnaryOperator op = toUnaryOperator(Consume(TokenType::Operator));
        auto expr = ParseExpression(getPrecedence(op) + 1);
        // AddressOf and Dereference are unary expressions, but we create different node
        // types for them; we will handle them much differently later
        if (op == UnaryOperator::AddressOf)
            return AddressOfExpression{ UE(expr) };
        else if (op == UnaryOperator::Dereference)
            return DereferenceExpression{ UE(expr) };
        else
            return UnaryExpression{ op, UE(expr), false };
    }

    // Cast expression
    if (next->type() == TokenType::Punctator && next->value() == "("
        && Peek(1)->type() == TokenType::Keyword) {
        LOG("CastExpression");
        Consume(TokenType::Punctator, "(");
        auto ret = CastExpression{};
        Type base_type = ParseTypes();
        AbstractDeclarator declarator = ParseAbstractDeclarator();
        ret.target_type = ProcessAbstractDeclarator(declarator, base_type);
        Consume(TokenType::Punctator, ")");
        ret.expr = unique_expression(ParseUnaryExpression());
        return ret;
    }

    return ParsePostfixExpression();
}

Expression ASTBuilder::ParsePostfixExpression()
{
    LOG("ParsePostfixExpression");
    Expression primary = ParsePrimaryExpression();
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == "[") {
        LOG("SubscriptExpression");
        while (next->type() == TokenType::Punctator && next->value() == "[") {
            Consume(TokenType::Punctator, "[");
            primary = SubscriptExpression{
                .pointer = UE(primary),
                .index = unique_expression(ParseExpression(0))
            };
            Consume(TokenType::Punctator, "]");
            next = Peek();
        }
    }
    return primary;
}

Expression ASTBuilder::ParsePrimaryExpression()
{
    LOG("ParsePrimaryExpression");
    auto next = Peek();

    if (next->type() == TokenType::Identifier) {
        next = Peek(1);
        if (next->type() == TokenType::Punctator && next->value() == "(")
            return ParseFunctionCall();
        LOG("VariableExpression");
        return VariableExpression{ Consume(TokenType::Identifier) };
    }

    if (next->type() == TokenType::Punctator && next->value() == "(") {
        LOG("( Expression )");
        Consume(TokenType::Punctator, "(");
        auto expr = ParseExpression(0);
        Consume(TokenType::Punctator, ")");
        return expr;
    }

    assert(next->type() == TokenType::NumericLiteral);
    return ParseConstantExpression();
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

Expression ASTBuilder::ParseConstantExpression()
{
    LOG("ParseConstantExpression");
    std::string literal = Consume(TokenType::NumericLiteral);
    // Parse suffixes
    // TODO: Maybe make different token types for different literals to skip this part here
    bool hasL = false;
    bool hasU = false;
    while (!literal.empty() && std::isalpha(literal.back())) {
        if (literal.back() == 'l' || literal.back() == 'L') {
            hasL = true;
            literal.pop_back();
        } else if (literal.back() == 'u' || literal.back() == 'U') {
            hasU = true;
            literal.pop_back();
        } else {
            // If we run into this, try to fix it in the lexer
            Abort(std::format("Unsupported '{}' in numeric literal", literal.back()));
        }
    }

    if (literal.contains('E') || literal.contains('e') || literal.contains('.')) {
        // Floating point
        // These numbers can have L suffixes, but we don't implement it.

        // TODO: Figure out what was the issue with strtod on Mac
#ifdef __APPLE__
        std::istringstream iss(literal);
        double value;
        iss >> value;
#else
        double value = std::strtod(literal.c_str(), nullptr);
#endif
        return ConstantExpression{ value };
    } else if (hasU) {
        // Unsigned
        unsigned long value = std::stoul(literal);
        if (hasL)
            return ConstantExpression{ value };
        if (value > std::numeric_limits<uint32_t>::min() &&
            value < std::numeric_limits<uint32_t>::max()) {
                return ConstantExpression{ static_cast<uint32_t>(value) };
        }
        return ConstantExpression{ value };
    } else {
        // Signed
        long value = std::stol(literal);
        if (hasL)
            return ConstantExpression{ value };
        if (value > std::numeric_limits<int>::min() &&
            value < std::numeric_limits<int>::max()) {
                return ConstantExpression{ static_cast<int>(value) };
        }
        return ConstantExpression{ value };
    }
}

uint64_t ASTBuilder::ParsePositiveInteger()
{
    // Expect a positive integer for array sizes
    std::string l = Consume(TokenType::NumericLiteral);
    if (l.contains('E') || l.contains('e') || l.contains('.'))
        Abort("Expected a positive integer, but a floating point literal found.");

    // Only keep the numbers in the literal
    l.erase(std::remove_if(l.begin(), l.end(), [](unsigned char c) {
        return !std::isdigit(c);
    }), l.end());

    uint64_t value = std::stoul(l);
    if (value == 0)
        Abort("Zero length arrays are not supported.");
    return value;
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
    if (next->value() != ";")
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
    if (IsStorageOrTypeSpecifier(next->value()))
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
    auto [storage, type] = ParseTypeSpecifierList();

    Declarator declarator = ParseDeclarator();
    LOG("ParseDeclarator END");
    const auto &[identifier, derived_type, param_names] = ProcessDeclarator(declarator, type);

    auto next = Peek();
    if (derived_type.isFunction()) {
        if (!allow_function)
            Abort("Function declaration is not allowed");
        // Function declaration
        auto func = FunctionDeclaration{};
        func.storage = storage;
        func.type = derived_type;
        func.name = std::move(identifier);
        func.params = param_names;
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
        decl.type = derived_type;
        decl.identifier = std::move(identifier);
        if (next->type() == TokenType::Punctator && next->value() == ";") {
            Consume(TokenType::Punctator, ";");
            return decl;
        }
        Consume(TokenType::Operator, "=");
        decl.init = std::make_unique<Initializer>(ParseInitializer());
        Consume(TokenType::Punctator, ";");
        return decl;
    }
    assert(false);
    return VariableDeclaration{};
}

Initializer ASTBuilder::ParseInitializer()
{
    LOG("ParseInitializer");
    Initializer ret;
    auto next = Peek();
    if (next->type() == TokenType::Punctator && next->value() == "{") {
        LOG("CompoundInit");
        CompoundInit init;
        Consume(TokenType::Punctator, "{");
        while (next->value() != "}") {
            init.list.push_back(std::make_unique<Initializer>(ParseInitializer()));
            next = Peek();
            if (next->type() == TokenType::Operator && next->value() == ",")
                Consume(TokenType::Operator, ",");
            next = Peek();
        }
        Consume(TokenType::Punctator, "}");
        ret.emplace<CompoundInit>(std::move(init));
    } else {
        LOG("SingleInit");
        SingleInit init{
            .expr = unique_expression(ParseExpression(0))
        };
        ret.emplace<SingleInit>(std::move(init));
    }
    return ret;
}

ASTBuilder::Declarator ASTBuilder::ParseDeclarator()
{
    // <declarator> ::= "*" <declarator> | <direct-declarator>
    // <direct-declarator> ::= <simple-declarator> [ <declarator-suffix> ]
    // <declarator-suffix> ::= <param-list> | { "[" <const> "]" }+
    // <simple-declarator> ::= <identifier> | "(" <declarator> ")"
    // <param-list> ::= "(" "void" ")" | "(" <param> { "," <param> } ")"
    // <param> ::= { <type-specifier> }+ <declarator>

    LOG("ParseDeclarator (recursive)");
    auto next = Peek();
    // <declarator> ::= "*" <declarator> | <direct-declarator>
    if (next->type() == TokenType::Operator && next->value() == "*") {
        // "*" <declarator>
        Consume(TokenType::Operator, "*");
        return PointerDeclarator{
            std::make_unique<Declarator>(ParseDeclarator())
        };
    } else {
        // <direct-declarator> ::= <simple-declarator> [ <declarator-suffix> ]
        Declarator direct_declarator;
        // <simple-declarator> ::= <identifier> | "(" <declarator> ")"
        Declarator simple_declarator;
        if (next->type() == TokenType::Punctator && next->value() == "(") {
            Consume(TokenType::Punctator, "(");
            simple_declarator = ParseDeclarator();
            Consume(TokenType::Punctator, ")");
        } else {
            std::string identifier = Consume(TokenType::Identifier);
            simple_declarator = IdentifierDeclarator{ identifier };
        }
        next = Peek();
        // <declarator-suffix> ::= <param-list> | { "[" <const> "]" }+
        if (next->type() == TokenType::Punctator && next->value() == "(") {
            // <param-list> ::= "(" "void" ")" | "(" <param> { "," <param> } ")"
            FunctionDeclarator func_declarator;
            func_declarator.inner_declarator = std::make_unique<Declarator>(std::move(simple_declarator));
            Consume(TokenType::Punctator, "(");
            if (Peek()->value() == "void" && Peek(1)->value() == ")")
                Consume(TokenType::Keyword, "void");
            else {
                while (next->value() != ")") {
                    if (next->value() == ",")
                        Consume(TokenType::Operator, ",");
                    // <param> ::= { <type-specifier> }+ <declarator>
                    func_declarator.params.emplace_back(DeclaratorParam{
                        ParseTypes(),
                        std::make_unique<Declarator>(ParseDeclarator())
                    });
                    next = Peek();
                }
            }
            Consume(TokenType::Punctator, ")");
            direct_declarator.emplace<FunctionDeclarator>(std::move(func_declarator));
        } else if (next->type() == TokenType::Punctator && next->value() == "[") {
            // { "[" <const> "]" }+
            // E.g.: array[1][2] -> ArrayDeclarator(ArrayDeclarator(ID("array"), 1), 2)
            Declarator outmost_declarator = Declarator{ std::move(simple_declarator) };
            while (Peek()->value() == "[") {
                ArrayDeclarator outer_arr;
                outer_arr.inner_declarator = std::make_unique<Declarator>(std::move(outmost_declarator));
                Consume(TokenType::Punctator, "[");
                outer_arr.size = ParsePositiveInteger();
                Consume(TokenType::Punctator, "]");
                outmost_declarator = Declarator{ std::move(outer_arr) };
            }
            direct_declarator = std::move(outmost_declarator);
        } else
            direct_declarator = std::move(simple_declarator);
        return direct_declarator;
    }
}

std::tuple<std::string, Type, std::vector<std::string>>
ASTBuilder::ProcessDeclarator(const Declarator &declarator, const Type &base_type)
{
    LOG("ProcessDeclarator (recursive)");
    if (auto id = std::get_if<IdentifierDeclarator>(&declarator))
        return std::make_tuple(id->identifier, base_type, std::vector<std::string>{});
    else if (auto ptr = std::get_if<PointerDeclarator>(&declarator)) {
        Type derived_type = Type{ PointerType{ std::make_shared<Type>(base_type) } };
        return ProcessDeclarator(*ptr->inner_declarator, derived_type);
    } else if (auto arr = std::get_if<ArrayDeclarator>(&declarator)) {
        Type derived_type = Type{ ArrayType{ std::make_shared<Type>(base_type), arr->size } };
        return ProcessDeclarator(*arr->inner_declarator, derived_type);
    } else if (auto func = std::get_if<FunctionDeclarator>(&declarator)) {
        if (auto func_id = std::get_if<IdentifierDeclarator>(func->inner_declarator.get())) {
            FunctionType func_type = FunctionType{
                .params = {},
                .ret = std::make_shared<Type>(base_type)
            };
            std::vector<std::string> param_names;
            for (auto &param : func->params) {
                const auto &[param_name, param_type, _] = ProcessDeclarator(*param.declarator, param.type);
                if (param_type.isFunction())
                    Abort("Function pointers are not supported yet.");
                param_names.push_back(param_name);
                func_type.params.push_back(std::make_shared<Type>(param_type));
            }
            return std::make_tuple(func_id->identifier, Type{ func_type }, param_names);
        } else
            Abort("Can't apply additional type derivations to a function type");
    } else
        assert(false);
    return std::make_tuple("", Type{}, std::vector<std::string>{});
}

ASTBuilder::AbstractDeclarator ASTBuilder::ParseAbstractDeclarator()
{
    // <abstract-declarator> ::= "*" [ <abstract-declarator> ]
    // | <direct-abstract-declarator>
    // <direct-abstract-declarator> ::= "(" <abstract-declarator> ")" { "[" <const> "]" }
    // | { "[" <const> "]" }+

    LOG("ParseAbstractDeclarator (recursive)");
    AbstractDeclarator abstract_declarator;
    auto next = Peek();
    if (next->type() == TokenType::Operator && next->value() == "*") {
        // "*" [ <abstract-declarator> ]
        Consume(TokenType::Operator, "*");
        AbstractPointerDeclarator ptr = AbstractPointerDeclarator{};
        ptr.inner_declarator = std::make_unique<AbstractDeclarator>(ParseAbstractDeclarator());
        abstract_declarator.emplace<AbstractPointerDeclarator>(std::move(ptr));
    } else if (next->type() == TokenType::Punctator && next->value() == "(") {
        // <direct-abstract-declarator> ::= "(" <abstract-declarator> ")" { "[" <const> "]" }
        Consume(TokenType::Punctator, "(");
        abstract_declarator = ParseAbstractDeclarator();
        Consume(TokenType::Punctator, ")");
        next = Peek();
        if (next->type() == TokenType::Punctator && next->value() == "[") {
            // { "[" <const> "]" } is zero or more [const]
            AbstractDeclarator outmost = AbstractDeclarator{ std::move(abstract_declarator) };
            while (Peek()->value() == "[") {
                AbstractArrayDeclarator outer_arr;
                outer_arr.inner_declarator = std::make_unique<AbstractDeclarator>(std::move(outmost));
                Consume(TokenType::Punctator, "[");
                outer_arr.size = ParsePositiveInteger();
                Consume(TokenType::Punctator, "]");
                outmost = AbstractDeclarator{ std::move(outer_arr) };
            }
            abstract_declarator = std::move(outmost);
        }
    } else if (next->type() == TokenType::Punctator && next->value() == "[") {
        // <direct-abstract-declarator> ::= ... | { "[" <const> "]" }+
        AbstractDeclarator outmost;
        outmost.emplace<AbstractBaseDeclarator>();
        while (Peek()->value() == "[") {
            AbstractArrayDeclarator outer_arr;
            outer_arr.inner_declarator = std::make_unique<AbstractDeclarator>(std::move(outmost));
            Consume(TokenType::Punctator, "[");
            outer_arr.size = ParsePositiveInteger();
            Consume(TokenType::Punctator, "]");
            outmost.emplace<AbstractArrayDeclarator>(std::move(outer_arr));
        }
        abstract_declarator = std::move(outmost);
    } else
        abstract_declarator.emplace<AbstractBaseDeclarator>();
    return abstract_declarator;
}

Type ASTBuilder::ProcessAbstractDeclarator(const AbstractDeclarator &decl, const Type &base_type)
{
    LOG("ProcessAbstractDeclarator (recursive)");
    if (std::holds_alternative<parser::ASTBuilder::AbstractBaseDeclarator>(decl)) {
        return base_type;
    } else if (auto ptr = std::get_if<AbstractPointerDeclarator>(&decl)) {
        Type derived_type = Type{ PointerType{ std::make_shared<Type>(base_type) } };
        return ProcessAbstractDeclarator(*ptr->inner_declarator, derived_type);
    } else if (auto arr = std::get_if<AbstractArrayDeclarator>(&decl)) {
        Type derived_type = Type{ ArrayType{ std::make_shared<Type>(base_type), arr->size } };
        return ProcessAbstractDeclarator(*arr->inner_declarator, derived_type);
    } else
        assert(false);
    return Type{};
}

std::pair<StorageClass, Type> ASTBuilder::ParseTypeSpecifierList()
{
    LOG("ParseTypeSpecifierList");
    std::set<std::string> type_specifiers;
    std::vector<StorageClass> storage_classes;
    std::optional<lexer::Token> next;
    for (next = Peek();
        next->type() == TokenType::Keyword && IsStorageOrTypeSpecifier(next->value());
        next = Peek()) {
        if (IsTypeSpecifier(next->value())) {
            auto [it, inserted] = type_specifiers.insert(next->value());
            if (!inserted)
                Abort(std::format("Duplicated type specifier '{}'", next->value()));
            Consume(TokenType::Keyword);
            continue;
        }

        if (auto storage = GetStorageClass(next->value())) {
            storage_classes.push_back(*storage);
            Consume(TokenType::Keyword);
            continue;
        }
    }

    std::optional<Type> type = DetermineType(type_specifiers);
    if (!type)
        Abort("Invalid type specification");

    if (storage_classes.size() > 1)
        Abort("Invalid storage class");
    StorageClass storage = storage_classes.empty() ? StorageClass::StorageDefault : storage_classes[0];

    return std::make_pair(storage, *type);
}

Type ASTBuilder::ParseTypes()
{
    LOG("ParseTypes");
    std::set<std::string> type_specifiers;
    for (std::optional<lexer::Token> next = Peek();
        next->type() == TokenType::Keyword && IsTypeSpecifier(next->value());
        next = Peek()) {
        auto [it, inserted] = type_specifiers.insert(next->value());
        if (!inserted)
            Abort(std::format("Duplicated type specifier '{}'", next->value()));
        Consume(TokenType::Keyword);
        continue;
    }

    std::optional<Type> type = DetermineType(type_specifiers);
    if (!type)
        Abort("Invalid type specification");
    return *type;
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
