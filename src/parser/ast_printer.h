#pragma once

#include "ast_visitor.h"

namespace parser {

struct ASTPrinter : public IASTVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    void operator()(const NumberExpression &e) override;
    void operator()(const VariableExpression &v) override;
    void operator()(const UnaryExpression &e) override;
    void operator()(const BinaryExpression &e) override;
    void operator()(const AssignmentExpression &a) override;
    void operator()(const ConditionalExpression &c) override;
    void operator()(const FuncDeclStatement &f) override;
    void operator()(const ReturnStatement &s) override;
    void operator()(const IfStatement &i) override;
    void operator()(const GotoStatement &g) override;
    void operator()(const LabeledStatement &l) override;
    void operator()(const BlockStatement &s) override;
    void operator()(const ExpressionStatement &e) override;
    void operator()(const NullStatement &) override;
    void operator()(const BreakStatement &b) override;
    void operator()(const ContinueStatement &c) override;
    void operator()(const WhileStatement &w) override;
    void operator()(const DoWhileStatement &d) override;
    void operator()(const ForStatement &f) override;
    void operator()(const Declaration &d) override;
    void operator()(std::monostate) override {}

    void print(const std::vector<parser::BlockItem> &root);
};

} // namespace parser
