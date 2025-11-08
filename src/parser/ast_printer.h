#pragma once

#include "ast_visitor.h"
#include <iostream>

namespace parser {

struct ASTPrinter : public IASTVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    // --- Expressions ---
    void operator()(const NumberExpression &e) override {
        pad(); std::cout << "NumberExpression(" << e.value << ")" << std::endl;
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

    // --- Statements ---
    void operator()(const FuncDeclStatement &f) override {
        pad(); std::cout << "Function(" << f.name << ")" << std::endl;
        indent += 2;
        pad(); std::cout << "Params: ";
        for (auto &p : f.params) std::cout << p << " ";
        std::cout << std::endl;
        std::visit(*this, *f.body);
        indent -= 2;
    }
    void operator()(const ReturnStatement &s) override {
        pad(); std::cout << "ReturnStatement" << std::endl;
        indent += 2; std::visit(*this, *s.expr); indent -= 2;
    }
    void operator()(const BlockStatement &s) override {
        pad(); std::cout << "Block" << std::endl;
        indent += 2;
        for (auto &stmt : s.statements)
            std::visit(*this, *stmt);
        indent -= 2;
    }
    void operator()(std::monostate) override {}
};

} // namespace parser
