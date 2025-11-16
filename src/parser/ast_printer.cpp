#include "ast_printer.h"
#include <iostream>

namespace parser {

void ASTPrinter::operator()(const NumberExpression &e)
{
    pad(); std::cout << "NumberExpression(" << e.value << ")" << std::endl;
}

void ASTPrinter::operator()(const VariableExpression &v)
{
    pad(); std::cout << "VariableExpression(" << v.identifier << ")" << std::endl;
}

void ASTPrinter::operator()(const UnaryExpression &e)
{
    pad(); std::cout << "UnaryExpression(" << toString(e.op) << std::endl;
    tab(); std::visit(*this, *e.expr); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const BinaryExpression &e)
{
    pad(); std::cout << "BinaryExpression(" << toString(e.op) << std::endl;
    tab();
    std::visit(*this, *e.lhs);
    std::visit(*this, *e.rhs);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const AssignmentExpression &a)
{
    pad(); std::cout << "AssignmentExpression(" << std::endl;
    tab();
    std::visit(*this, *a.lhs);
    std::visit(*this, *a.rhs);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const ConditionalExpression &c)
{
    pad(); std::cout << "ConditionalExpression(" << std::endl;
    tab();
    pad(); std::cout << "If" << std::endl;
    std::visit(*this, *c.condition);
    pad(); std::cout << "Then" << std::endl;
    std::visit(*this, *c.trueBranch);
    pad(); std::cout << "Else" << std::endl;
    std::visit(*this, *c.falseBranch);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const FuncDeclStatement &f)
{
    pad(); std::cout << "Function(" << f.name << ")" << std::endl;
    tab();
    pad(); std::cout << "Params: ";
    for (auto &p : f.params) std::cout << p << " ";
    std::cout << std::endl;
    for (auto &i : f.body)
        std::visit(*this, i);
        shift_tab();
}

void ASTPrinter::operator()(const ReturnStatement &s)
{
    pad(); std::cout << "Return" << std::endl;
    tab(); std::visit(*this, *s.expr); shift_tab();
}

void ASTPrinter::operator()(const IfStatement &i)
{
    pad(); std::cout << "If(" << std::endl;
    tab();
    std::visit(*this, *i.condition);
    pad(); std::cout << "Then" << std::endl;
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch) {
        pad(); std::cout << "Else" << std::endl;
        std::visit(*this, *i.falseBranch);
    }
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const BlockStatement &s)
{
    pad(); std::cout << "Block" << std::endl;
    tab();
    for (auto &stmt : s.statements)
        std::visit(*this, *stmt);
    shift_tab();
}

void ASTPrinter::operator()(const ExpressionStatement &e)
{
    pad(); std::cout << "Expression" << std::endl;
    tab(); std::visit(*this, *e.expr); shift_tab();
}

void ASTPrinter::operator()(const NullStatement &)
{
    pad(); std::cout << "Null" << std::endl;
}

void ASTPrinter::operator()(const Declaration &d)
{
    pad(); std::cout << "Declaration(" << d.identifier << ")" << std::endl;
    if (d.init.get() != 0) {
        pad(); std::cout << "Init: " << std::endl;
        tab(); std::visit(*this, *d.init); shift_tab();
    }
}

void ASTPrinter::print(const std::vector<parser::BlockItem> &root)
{
    for (auto &i: root)
        std::visit(*this, i);
}

} // namespace parser
