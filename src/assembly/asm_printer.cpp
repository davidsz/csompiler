#include "asm_printer.h"
#include <cassert>

namespace assembly {

static std::string getEightByteName(Register reg)
{
    switch (reg) {
#define CASE_TO_STRING(name, eightbytename, fourbytename, onebytename) \
    case Register::name: return eightbytename;
    ASM_REGISTER_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

static std::string getFourByteName(Register reg)
{
    switch (reg) {
#define CASE_TO_STRING(name, eightbytename, fourbytename, onebytename) \
    case Register::name: return fourbytename;
    ASM_REGISTER_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

static std::string getOneByteName(Register reg)
{
    switch (reg) {
#define CASE_TO_STRING(name, eightbytename, fourbytename, onebytename) \
    case Register::name: return onebytename;
    ASM_REGISTER_LIST(CASE_TO_STRING)
#undef CASE_TO_STRING
    }
    assert(false);
    return "";
}

ASMPrinter::ASMPrinter(std::shared_ptr<ASMSymbolTable> symbolTable)
    : m_symbolTable(symbolTable)
{
}

void ASMPrinter::operator()(const Reg &r)
{
    switch (r.bytes) {
    case 1:
        m_codeStream << "%" << getOneByteName(r.reg);
        break;
    case 4:
        m_codeStream << "%" << getFourByteName(r.reg);
        break;
    case 8:
        m_codeStream << "%" << getEightByteName(r.reg);
        break;
    default:
        m_codeStream << "UNKNOWN_REGISTER";
        break;
    }
}

void ASMPrinter::operator()(const Imm &i)
{
    m_codeStream << "$" << std::to_string(i.value);
}

void ASMPrinter::operator()(const Pseudo &)
{
    // Something is wrong if you see this in Assembly
    m_codeStream << "!!!PSEUDO!!!";
}

void ASMPrinter::operator()(const Stack &s)
{
    m_codeStream << s.offset << "(%rbp)";
}

void ASMPrinter::operator()(const Data &d)
{
    // TODO: No "_" on Linux
    m_codeStream << "_" << d.name << "(%rip)";
}

void ASMPrinter::operator()(const Comment &c)
{
    m_codeStream << "    # " << c.text << std::endl;
}

void ASMPrinter::operator()(const Mov &m)
{
    if (m.type == WordType::Longword)
        m_codeStream << "    movl ";
    else
        m_codeStream << "    movq ";
    std::visit(*this, m.src);
    m_codeStream << ", ";
    std::visit(*this, m.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Movsx &m)
{
    // The MovsX instruction takes suffixes for both its source
    // and destination operand sizes.
    m_codeStream << "    movslq ";
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

    m_codeStream << "    ret" << std::endl << std::endl;
}

void ASMPrinter::operator()(const Unary &u)
{
    m_codeStream << "    " << toString(u.op, u.type == Longword) << " ";
    std::visit(*this, u.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Binary &b)
{
    m_codeStream << "    " << toString(b.op, b.type == Longword) << " ";
    std::visit(*this, b.src);
    m_codeStream << ", ";
    std::visit(*this, b.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Idiv &d)
{
    if (d.type == WordType::Longword)
        m_codeStream << "    idivl ";
    else
        m_codeStream << "    idivq ";
    std::visit(*this, d.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Cdq &c)
{
    if (c.type == WordType::Longword)
        m_codeStream << "    cdq" << std::endl;
    else
        m_codeStream << "    cqo" << std::endl;
}

void ASMPrinter::operator()(const Cmp &c)
{
    if (c.type == WordType::Longword)
        m_codeStream << "    cmpl ";
    else
        m_codeStream << "    cmpq ";
    std::visit(*this, c.lhs);
    m_codeStream << ", ";
    std::visit(*this, c.rhs);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Jmp &j)
{
    m_codeStream << "    jmp L" << j.identifier << std::endl;
}

void ASMPrinter::operator()(const JmpCC &j)
{
    m_codeStream << "    j" << j.cond_code << " L" << j.identifier << std::endl;
}

void ASMPrinter::operator()(const SetCC &s)
{
    m_codeStream << "    set" << s.cond_code << " ";
    std::visit(*this, s.op);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Label &l)
{
    m_codeStream << "L" << l.identifier << ": " << std::endl;
}

void ASMPrinter::operator()(const Push &p)
{
    m_codeStream << "    pushq ";
    std::visit(*this, p.op);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Call &c)
{
    m_codeStream << "    call _" << c.identifier << std::endl;
}

void ASMPrinter::operator()(const Function &f)
{
    // TODO: Annotating with _ is specific to MacOS
    if (f.global)
        m_codeStream << ".globl _" << f.name << std::endl;
    m_codeStream << ".text" << std::endl;
    m_codeStream << "_" << f.name << ":" << std::endl;

    // Prologue
    m_codeStream << "    pushq %rbp" << std::endl;
    m_codeStream << "    movq %rsp, %rbp" << std::endl;
    m_codeStream << "    subq $" << f.stackSize << ", %rsp" << std::endl;
    m_codeStream << std::endl;

    for (auto &i: f.instructions)
        std::visit(*this, i);
}

void ASMPrinter::operator()(const StaticVariable &s)
{
    // TODO: Annotating with _ is specific to MacOS
    if (s.global)
        m_codeStream << ".globl _" << s.name << std::endl;

    long s_init = forceLong(s.init);
    bool is_long = std::holds_alternative<int>(s.init);
    if (s_init == 0)
        m_codeStream << ".bss" << std::endl;
    else
        m_codeStream << ".data" << std::endl;

    m_codeStream << ".balign " << s.alignment << std::endl;
    m_codeStream << "_" << s.name << ":" << std::endl;

    if (s_init == 0) {
        if (is_long)
            m_codeStream << ".zero 4" << std::endl;
        else
            m_codeStream << ".zero 8" << std::endl;
    } else {
        if (is_long)
            m_codeStream << ".long " << s_init << std::endl;
        else
            m_codeStream << ".quad " << s_init << std::endl;
    }
}

void ASMPrinter::operator()(std::monostate)
{
    assert(false);
}

std::string ASMPrinter::ToText(std::list<TopLevel> instructions)
{
    for (auto &i: instructions)
        std::visit(*this, i);
    return m_codeStream.str();
}

}; // assembly
