#include "asm_printer.h"
#include <cassert>

namespace assembly {

void ASMPrinter::operator()(const Reg &r)
{
    m_codeStream << "%" << r.name;
}

void ASMPrinter::operator()(const Imm &i)
{
    m_codeStream << "$" << std::to_string(i.value);
}

void ASMPrinter::operator()(const Pseudo &)
{ m_codeStream << "!!!PSEUDO!!!"; }

void ASMPrinter::operator()(const Stack &s)
{
    m_codeStream << s.offset << "(%rbp)";
}

void ASMPrinter::operator()(const Mov &m)
{
    m_codeStream << "    movl ";
    std::visit(*this, m.src);
    m_codeStream << ", ";
    std::visit(*this, m.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Ret &)
{
    // Epilogue
    m_codeStream << std::endl;
    m_codeStream << "    movq %rbp, %rsp" << std::endl;
    m_codeStream << "    popq %rbp" << std::endl;

    m_codeStream << "    ret" << std::endl;
}

void ASMPrinter::operator()(const Unary &u)
{
    m_codeStream << "    " << toString(u.op) << " ";
    std::visit(*this, u.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Function &f)
{
    m_codeStream << "    .global _" << f.name << std::endl;
    m_codeStream << "_" << f.name << ":" << std::endl;

    // Prologue
    m_codeStream << "    pushq %rbp" << std::endl;
    m_codeStream << "    movq %rsp, %rbp" << std::endl;
    m_codeStream << "    subq $" << f.stackSize << ", %rsp" << std::endl;
    m_codeStream << std::endl;

    for (auto &i: f.instructions)
        std::visit(*this, i);
}

void ASMPrinter::operator()(std::monostate)
{
    assert(false);
}

std::string ASMPrinter::ToText(std::vector<Instruction> instructions)
{
    for (auto &i: instructions)
        std::visit(*this, i);
    return m_codeStream.str();
}

}; // assembly
