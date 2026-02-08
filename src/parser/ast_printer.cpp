#include "ast_printer.h"
#include <iostream>

namespace parser {

void ASTPrinter::operator()(const ConstantExpression &e)
{
    pad();
    std::cout << "ConstantExpression(" << toString(e.value) << ") ";
    std::cout << e.type << std::endl;
}

void ASTPrinter::operator()(const StringExpression &s)
{
    pad();
    std::cout << "StringExpression(" << s.value << ") ";
    std::cout << s.type << std::endl;
}

void ASTPrinter::operator()(const VariableExpression &v)
{
    pad();
    std::cout << "VariableExpression(" << v.identifier << ") ";
    std::cout << v.type << std::endl;
}

void ASTPrinter::operator()(const CastExpression &c)
{
    pad(); std::cout << "CastExpression(";
    std::cout << c.inner_type << " -> " << c.type << std::endl;
    tab(); std::visit(*this, *c.expr); shift_tab();
    pad(); std::cout << ") " << std::endl;
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

void ASTPrinter::operator()(const CompoundAssignmentExpression &c)
{
    pad(); std::cout << "CompoundAssignmentExpression(" << toString(c.op) << " ";
    std::cout << c.inner_type << " -> " << c.type << std::endl;
    tab();
    std::visit(*this, *c.lhs);
    std::visit(*this, *c.rhs);
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
    pad(); std::cout << ") ";
    std::cout << f.type << std::endl;
}

void ASTPrinter::operator()(const DereferenceExpression &d)
{
    pad(); std::cout << "DereferenceExpression( " << d.type << std::endl;
    tab(); std::visit(*this, *d.expr); shift_tab();
    pad(); std::cout << ") " << std::endl;
}

void ASTPrinter::operator()(const AddressOfExpression &a)
{
    pad(); std::cout << "AddressOfExpression( " << a.type << std::endl;
    tab(); std::visit(*this, *a.expr); shift_tab();
    pad(); std::cout << ") " << std::endl;
}

void ASTPrinter::operator()(const SubscriptExpression &s)
{
    pad(); std::cout << "SubscriptExpression( " << s.type << std::endl;
    tab();
    std::visit(*this, *s.pointer);
    std::visit(*this, *s.index);
    shift_tab();
    pad(); std::cout << ") " << std::endl;
}

void ASTPrinter::operator()(const SizeOfExpression &s)
{
    pad(); std::cout << "SizeOfExpression( " << s.type << std::endl;
    tab();
    std::visit(*this, *s.expr);
    shift_tab();
    pad(); std::cout << ") " << std::endl;
}

void ASTPrinter::operator()(const SizeOfTypeExpression &s)
{
    pad(); std::cout << "SizeOfTypeExpression( " << s.type << std::endl;
    tab();
    pad(); std::cout << s.operand << std::endl;
    shift_tab();
    pad(); std::cout << ") " << std::endl;
}

void ASTPrinter::operator()(const ReturnStatement &r)
{
    pad(); std::cout << "Return" << std::endl;
    if (r.expr) {
        tab(); std::visit(*this, *r.expr); shift_tab();
    }
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
    pad(); std::cout << "ExpressionStatement" << std::endl;
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

void ASTPrinter::operator()(const VariableDeclaration &v)
{
    pad(); std::cout << "VariableDeclaration(" << v.identifier << ") " << v.type << std::endl;
    if (v.init) {
        pad(); std::cout << "Init: " << std::endl;
        tab(); std::visit(*this, *v.init); shift_tab();
    }
}

void ASTPrinter::operator()(const SingleInit &s)
{
    std::visit(*this, *s.expr);
}

void ASTPrinter::operator()(const CompoundInit &c)
{
    pad(); std::cout << "{" << std::endl;
    tab();
    for (size_t i = 0; i < c.list.size(); i++) {
        std::visit(*this, *c.list[i]);
        if (i == 2) {
            pad(); std::cout << "... " << c.list.size() << " elements" << std::endl;
            break;
        }
    }
    shift_tab();
    pad(); std::cout << "}" << std::endl;
}

void ASTPrinter::print(const std::vector<Declaration> &root)
{
    for (auto &i: root)
        std::visit(*this, i);
}

} // namespace parser
