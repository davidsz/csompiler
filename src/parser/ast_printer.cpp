#include "ast_printer.h"
#include <iostream>

namespace parser {

void ASTPrinter::operator()(const ConstantExpression &e)
{
    pad();
    std::cout << "ConstantExpression(" << toString(e.value) << ") ";
    std::cout << e.type << std::endl;
}

void ASTPrinter::operator()(const VariableExpression &v)
{
    pad();
    std::cout << "VariableExpression(" << v.identifier << ") ";
    std::cout << v.type << std::endl;
}

void ASTPrinter::operator()(const CastExpression &c)
{
    pad(); std::cout << "CastExpression(" << c.target_type << std::endl;
    tab(); std::visit(*this, *c.expr); shift_tab();
    pad(); std::cout << ") " << c.type << std::endl;
}

void ASTPrinter::operator()(const UnaryExpression &u)
{
    pad(); std::cout << "UnaryExpression(" << toString(u.op) << std::endl;
    tab(); std::visit(*this, *u.expr); shift_tab();
    pad(); std::cout << ") " << u.type << std::endl;
}

void ASTPrinter::operator()(const BinaryExpression &b)
{
    pad(); std::cout << "BinaryExpression(" << toString(b.op) << std::endl;
    tab();
    std::visit(*this, *b.lhs);
    std::visit(*this, *b.rhs);
    shift_tab();
    pad(); std::cout << ") " << b.type << std::endl;
}

void ASTPrinter::operator()(const AssignmentExpression &a)
{
    pad(); std::cout << "AssignmentExpression(" << std::endl;
    tab();
    std::visit(*this, *a.lhs);
    std::visit(*this, *a.rhs);
    shift_tab();
    pad(); std::cout << ") " << a.type << std::endl;
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
    pad(); std::cout << ") " << c.type << std::endl;
}

void ASTPrinter::operator()(const FunctionCallExpression &f)
{
    pad(); std::cout << "FunctionCallExpression(" << std::endl;
    tab();
    pad(); std::cout << "Identifier: " << f.identifier << std::endl;
    pad(); std::cout << "Args" << std::endl;
    for (auto &a : f.args)
        std::visit(*this, *a);
    shift_tab();
    pad(); std::cout << ") " << f.type << std::endl;
}

void ASTPrinter::operator()(const ReturnStatement &r)
{
    pad(); std::cout << "Return" << std::endl;
    tab(); std::visit(*this, *r.expr); shift_tab();
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

void ASTPrinter::operator()(const GotoStatement &g)
{
    pad(); std::cout << "Goto(" << g.label << ")" << std::endl;
}

void ASTPrinter::operator()(const LabeledStatement &l)
{
    pad(); std::cout << "Label(" << l.label << "):" << std::endl;
    tab();
    std::visit(*this, *l.statement);
    shift_tab();
}

void ASTPrinter::operator()(const BlockStatement &s)
{
    pad(); std::cout << "Block" << std::endl;
    tab();
    for (auto &item : s.items)
        std::visit(*this, item);
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

void ASTPrinter::operator()(const BreakStatement &)
{
    pad(); std::cout << "Break" << std::endl;
}

void ASTPrinter::operator()(const ContinueStatement &)
{
    pad(); std::cout << "Continue" << std::endl;
}

void ASTPrinter::operator()(const WhileStatement &w)
{
    pad(); std::cout << "While(" << std::endl;
    tab(); std::visit(*this, *w.condition); shift_tab();
    pad(); std::cout << "Block" << std::endl;
    tab(); std::visit(*this, *w.body); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const DoWhileStatement &d)
{
    pad(); std::cout << "Do(" << std::endl;
    tab(); std::visit(*this, *d.body); shift_tab();
    pad(); std::cout << "While" << std::endl;
    tab(); std::visit(*this, *d.condition); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const ForStatement &f)
{
    pad(); std::cout << "For(" << std::endl;
    if (f.init) {
        pad(); std::cout << "Init" << std::endl;
        tab(); std::visit(*this, *f.init); shift_tab();
    }
    if (f.condition) {
        pad(); std::cout << "Condition" << std::endl;
        tab(); std::visit(*this, *f.condition); shift_tab();
    }
    if (f.update) {
        pad(); std::cout << "Update" << std::endl;
        tab(); std::visit(*this, *f.update); shift_tab();
    }
    pad(); std::cout << "Body" << std::endl;
    tab(); std::visit(*this, *f.body); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const SwitchStatement &s)
{
    pad(); std::cout << "Switch(" << std::endl;
    pad(); std::cout << "Condition" << std::endl;
    tab(); std::visit(*this, *s.condition); shift_tab();
    pad(); std::cout << "Body" << std::endl;
    tab(); std::visit(*this, *s.body); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const CaseStatement &c)
{
    pad(); std::cout << "Case(" << std::endl;
    pad(); std::cout << "Condition" << std::endl;
    tab(); std::visit(*this, *c.condition); shift_tab();
    pad(); std::cout << "Statement" << std::endl;
    tab(); std::visit(*this, *c.statement); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const DefaultStatement &d)
{
    pad(); std::cout << "Default(" << std::endl;
    pad(); std::cout << "Statement" << std::endl;
    tab(); std::visit(*this, *d.statement); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void ASTPrinter::operator()(const FunctionDeclaration &f)
{
    pad(); std::cout << "FunctionDeclaration(" << f.name << ")" << std::endl;
    tab();
    pad(); std::cout << "Params: ";
    for (auto &p : f.params) std::cout << p << " ";
    std::cout << std::endl;
    if (f.body)
        std::visit(*this, *f.body);
    shift_tab();
}

void ASTPrinter::operator()(const VariableDeclaration &d)
{
    pad(); std::cout << "VariableDeclaration(" << d.identifier << ")" << std::endl;
    if (d.init.get() != 0) {
        pad(); std::cout << "Init: " << std::endl;
        tab(); std::visit(*this, *d.init); shift_tab();
    }
}

void ASTPrinter::print(const std::vector<Declaration> &root)
{
    for (auto &i: root)
        std::visit(*this, i);
}

} // namespace parser
