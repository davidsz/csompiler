#pragma once

#include "ast_visitor.h"
#include <iostream>

namespace parser {

struct ASTPrinter : public IASTVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    void operator()(const NumberExpression &e) override {
        pad(); std::cout << "NumberExpression(" << e.value << ")" << std::endl;
    }
    void operator()(const VariableExpression &v) override {
        pad(); std::cout << "VariableExpression(" << v.identifier << ")" << std::endl;
    }
    void operator()(const UnaryExpression &e) override {
        pad(); std::cout << "UnaryExpression(" << toString(e.op) << std::endl;
        tab(); std::visit(*this, *e.expr); shift_tab();
        pad(); std::cout << ")" << std::endl;
    }
    void operator()(const BinaryExpression &e) override {
        pad(); std::cout << "BinaryExpression(" << toString(e.op) << std::endl;
        tab();
        std::visit(*this, *e.lhs);
        std::visit(*this, *e.rhs);
        shift_tab();
        pad(); std::cout << ")" << std::endl;
    }
    void operator()(const AssignmentExpression &a) override {
        pad(); std::cout << "AssignmentExpression(" << std::endl;
        tab();
        std::visit(*this, *a.lhs);
        std::visit(*this, *a.rhs);
        shift_tab();
        pad(); std::cout << ")" << std::endl;
    }

    // --- Statements ---
    void operator()(const FuncDeclStatement &f) override {
        pad(); std::cout << "Function(" << f.name << ")" << std::endl;
        tab();
        pad(); std::cout << "Params: ";
        for (auto &p : f.params) std::cout << p << " ";
        std::cout << std::endl;
        for (auto &i : f.body)
            std::visit(*this, i);
            shift_tab();
    }
    void operator()(const ReturnStatement &s) override {
        pad(); std::cout << "Return" << std::endl;
        tab(); std::visit(*this, *s.expr); shift_tab();
    }
    void operator()(const BlockStatement &s) override {
        pad(); std::cout << "Block" << std::endl;
        tab();
        for (auto &stmt : s.statements)
            std::visit(*this, *stmt);
        shift_tab();
    }
    void operator()(const ExpressionStatement &e) override {
        pad(); std::cout << "Expression" << std::endl;
        tab(); std::visit(*this, *e.expr); shift_tab();
    }
    void operator()(const NullStatement &) override {
        pad(); std::cout << "Null" << std::endl;
    }

    void operator()(const Declaration &d) override {
        pad(); std::cout << "Declaration(" << d.identifier << ")" << std::endl;
        if (d.init.get() != 0) {
            pad(); std::cout << "Init: " << std::endl;
            tab(); std::visit(*this, *d.init); shift_tab();
        }
    }

    void operator()(std::monostate) override {}

    void print(const std::vector<parser::BlockItem> &root)
    {
        for (auto &i: root)
            std::visit(*this, i);
    }
};

} // namespace parser
