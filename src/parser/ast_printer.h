#ifndef AST_PRINTER_H
#define AST_PRINTER_H

#include "ast_visitor.h"
#include <iostream>

namespace parser {

struct ASTPrinter : public IASTVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }

    // --- Expressions ---
    void operator()(const NumberExpression &e) override {
        pad(); std::cout << "Number(" << e.value << ")" << std::endl;
    }
    void operator()(const VariableExpression &e) override {
        pad(); std::cout << "Variable(" << e.name << ")" << std::endl;
    }
    void operator()(const BinaryExpression &e) override {
        pad(); std::cout << "Binary(" << e.op << ")" << std::endl;;
        indent += 2;
        std::visit(*this, *e.lhs);
        std::visit(*this, *e.rhs);
        indent -= 2;
    }
    void operator()(const CallExpression &e) override {
        pad(); std::cout << "Call(" << e.callee << ")" << std::endl;
        indent += 2;
        for (auto &a : e.args)
            std::visit(*this, *a);
        indent -= 2;
    }

    // --- Statements ---
    void operator()(const VarDeclStatement &s) override {
        pad(); std::cout << "VarDeclStatement(" << s.name << ")" << std::endl;
        indent += 2;
        if (s.init) std::visit(*this, *s.init);
        indent -= 2;
    }
    void operator()(const FuncDeclStatement &f) override {
        pad(); std::cout << "Function(" << f.name << ")" << std::endl;
        indent += 2;
        pad(); std::cout << "Params: ";
        for (auto &p : f.params) std::cout << p << " ";
        std::cout << std::endl;
        std::visit(*this, *f.body);
        indent -= 2;
    }
    void operator()(const ExpressionStatement &s) override {
        pad(); std::cout << "ExpressionStatement" << std::endl;
        indent += 2; std::visit(*this, *s.expr); indent -= 2;
    }
    void operator()(const ReturnStatement &s) override {
        pad(); std::cout << "ReturnStatement" << std::endl;
        indent += 2; std::visit(*this, *s.expr); indent -= 2;
    }
    void operator()(const IfStatement &s) override {
        pad(); std::cout << "If" << std::endl;
        indent += 2;
        pad(); std::cout << "Cond:" << std::endl; indent += 2; std::visit(*this, *s.condition); indent -= 2;
        pad(); std::cout << "Then:" << std::endl; indent += 2; std::visit(*this, *s.thenBranch); indent -= 2;
        pad(); std::cout << "Else:" << std::endl; indent += 2; std::visit(*this, *s.elseBranch); indent -= 2;
        indent -= 2;
    }
    void operator()(const BlockStatement &s) override {
        pad(); std::cout << "Block" << std::endl;
        indent += 2;
        for (auto &stmt : s.statements)
            std::visit(*this, *stmt);
        indent -= 2;
    }
};

} // namespace parser

#endif // AST_PRINTER_H
