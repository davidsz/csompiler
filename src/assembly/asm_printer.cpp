#include "asm_printer.h"
#include <algorithm>
#include <cassert>

namespace assembly {

static std::string getInitializer(WordType type)
{
    switch (type) {
    case Longword:   return ".long";
    case Quadword:   return ".quad";
    case Doubleword: return ".double";
    default: assert(false); return "ERROR";
    }
}

static std::string formatLabel(std::string_view name)
{
#ifdef __APPLE__
    return std::format("_{}", name);
#else
    return name;
#endif
}

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

void ASMPrinter::operator()(const PseudoAggregate &)
{
    // Something is wrong if you see this in Assembly
    m_codeStream << "!!!PSEUDO_AGGREGATE!!!";
}

void ASMPrinter::operator()(const Memory &m)
{
    m_codeStream << m.offset << "(%" << getEightByteName(m.reg) << ")";
}

void ASMPrinter::operator()(const Data &d)
{
    // TODO: Append L?
    m_codeStream << formatLabel(d.name) << "(%rip)";
}

void ASMPrinter::operator()(const Indexed &i)
{
    m_codeStream << "(";
    m_codeStream << "%" << getEightByteName(i.base) << ", ";
    m_codeStream << "%" << getEightByteName(i.index) << ", ";
    m_codeStream << std::to_string(i.scale) << ")";
}

void ASMPrinter::operator()(const Comment &c)
{
    m_codeStream << "    # " << c.text << std::endl;
}

void ASMPrinter::operator()(const Mov &m)
{
    m_codeStream << "    " << AddSuffix("mov", m.type) << " ";
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

void ASMPrinter::operator()(const MovZeroExtend &)
{
    // Replaced during instruction fixup
    assert(false);
}

void ASMPrinter::operator()(const Lea &l)
{
    m_codeStream << "    leaq ";
    std::visit(*this, l.src);
    m_codeStream << ", ";
    std::visit(*this, l.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Cvttsd2si &c)
{
    m_codeStream << "    " << AddSuffix("cvttsd2si", c.type) << " ";
    std::visit(*this, c.src);
    m_codeStream << ", ";
    std::visit(*this, c.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Cvtsi2sd &c)
{
    m_codeStream << "    " << AddSuffix("cvtsi2sd", c.type) << " ";
    std::visit(*this, c.src);
    m_codeStream << ", ";
    std::visit(*this, c.dst);
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
    m_codeStream << "    " << toString(u.op, u.type) << " ";
    std::visit(*this, u.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Binary &b)
{
    m_codeStream << "    " << toString(b.op, b.type) << " ";
    std::visit(*this, b.src);
    m_codeStream << ", ";
    std::visit(*this, b.dst);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Idiv &i)
{
    m_codeStream << "    " << AddSuffix("idiv", i.type) << " ";
    std::visit(*this, i.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Div &d)
{
    m_codeStream << "    " << AddSuffix("div", d.type) << " ";
    std::visit(*this, d.src);
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const Cdq &c)
{
    if (c.type == WordType::Longword)
        m_codeStream << "    cdq" << std::endl;
    else if (c.type == Quadword || c.type == Doubleword)
        m_codeStream << "    cqo" << std::endl;
    else
        assert(false);
}

void ASMPrinter::operator()(const Cmp &c)
{
    if (c.type == Doubleword)
        m_codeStream << "    comisd ";
    else if (c.type == Longword || c.type == Quadword)
        m_codeStream << "    " << AddSuffix("cmp", c.type) << " ";
    else
        assert(false);
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
    m_codeStream << "    call " << formatLabel(c.identifier) << std::endl;
}

void ASMPrinter::operator()(const Function &f)
{
    if (f.global)
        m_codeStream << "    .globl " << formatLabel(f.name) << std::endl;

    m_codeStream << "    .text" << std::endl;

    m_codeStream << formatLabel(f.name) << ":" << std::endl;

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
    if (s.global)
        m_codeStream << "    .globl " << formatLabel(s.name) << std::endl;

    bool isZero = std::ranges::all_of(s.list, [&](ConstantValue v) {
        return std::holds_alternative<ZeroBytes>(v) || isPositiveZero(v);
    });
    if (!isZero)
        m_codeStream << "    .data" << std::endl;
    else
        m_codeStream << "    .bss" << std::endl;

    m_codeStream << "    .balign " << s.alignment << std::endl;

    m_codeStream << formatLabel(s.name) << ":" << std::endl;

    for (auto &i : s.list) {
        // TODO: Create a function to avoid duplication of this condition
        if (auto z = std::get_if<ZeroBytes>(&i))
            m_codeStream << "    .zero " << z->bytes << std::endl;
        else
            m_codeStream << "    " << getInitializer(getType(i).wordType()) << " " << toString(i) << std::endl;
    }
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(const StaticConstant &s)
{
#ifdef __APPLE__
    m_codeStream << "    .literal" << s.alignment << std::endl;
    m_codeStream << "    .balign " << s.alignment << std::endl;
#else
    m_codeStream << "    .section .rodata" << std::endl;
    m_codeStream << "    .balign " << s.alignment << std::endl;
#endif

    m_codeStream << formatLabel(s.name) << ":" << std::endl;

    Type type = getType(s.init);
    if (isPositiveZero(s.init))
        m_codeStream << "    .zero " << type.size() << std::endl;
    else
        m_codeStream << "    " << getInitializer(type.wordType()) << " " << toString(s.init) << std::endl;

#ifdef __APPLE__
    if (s.alignment == 16)
        m_codeStream << "    .quad 0" << std::endl;
#endif
    m_codeStream << std::endl;
}

void ASMPrinter::operator()(std::monostate)
{
    assert(false);
}

std::string ASMPrinter::ToText(std::list<TopLevel> instructions)
{
    for (auto &i: instructions)
        std::visit(*this, i);

    // Disallow executable stack
#ifdef __linux__
    m_codeStream << std::endl << ".section .note.GNU-stack,\"\",@progbits" << std::endl;
#endif

    return m_codeStream.str();
}

}; // assembly
