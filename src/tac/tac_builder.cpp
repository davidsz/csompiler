#include "tac_builder.h"

namespace tac {

Value TACBuilder::operator()(const parser::NumberExpression &n)
{
    return Constant{ static_cast<int>(n.value) };
}

Value TACBuilder::operator()(const parser::UnaryExpression &u)
{
    auto unary = Unary{};
    unary.op = u.op;
    unary.src = std::visit(*this, *u.expr);
    Variant dst{ "dst_temp" };
    unary.dst = dst;
    m_instructions.push_back(unary);
    return dst;
}

Value TACBuilder::operator()(const parser::FuncDeclStatement &f)
{
    auto func = FunctionDefinition{};
    func.name = f.name;
    TACBuilder builder;
    auto &block = std::get<parser::BlockStatement>(*f.body);
    func.inst = builder.Convert(&block);
    m_instructions.push_back(func);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ReturnStatement &r)
{
    auto ret = Return{};
    ret.val = std::visit(*this, *r.expr);
    m_instructions.push_back(ret);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::BlockStatement &b)
{
    for (auto &s : b.statements)
        std::visit(*this, *s);
    return std::monostate();
}

Value TACBuilder::operator()(std::monostate)
{
    assert(false);
    return std::monostate();
}

std::vector<Instruction> TACBuilder::Convert(parser::BlockStatement *b) {
    (*this)(*b);
    return m_instructions;
}

}; // tac
