#include "tac_printer.h"
#include "common/context.h"
#include <iostream>

namespace tac {

TACPrinter::TACPrinter(Context *context)
    : m_context(context)
{
}

void TACPrinter::operator()(const tac::Constant &c)
{
    pad();
    std::cout << "Constant(" << toString(c.value) << ") ";
    std::cout << getType(c.value) << std::endl;
}

void TACPrinter::operator()(const tac::Variant &v)
{
    pad();
    std::cout << "Variant(" << v.name << ") ";
    const SymbolEntry *entry = m_context->symbolTable->get(v.name);
    std::cout << entry->type << std::endl;
}

void TACPrinter::operator()(const tac::Return &r)
{
    pad(); std::cout << "Return(" << std::endl;
    if (r.val) {
        tab(); std::visit(*this, *r.val); shift_tab();
    }
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Unary &u)
{
    pad(); std::cout << "Unary(" << toString(u.op) << std::endl;
    tab();
    std::visit(*this, u.src);
    std::visit(*this, u.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Binary &b)
{
    pad(); std::cout << "Binary(" << toString(b.op) << std::endl;
    tab();
    std::visit(*this, b.src1);
    std::visit(*this, b.src2);
    std::visit(*this, b.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Copy &c)
{
    pad(); std::cout << "Copy(" << std::endl;
    tab();
    std::visit(*this, c.src);
    std::visit(*this, c.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::GetAddress &g)
{
    pad(); std::cout << "GetAddress(" << std::endl;
    tab();
    std::visit(*this, g.src);
    std::visit(*this, g.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Load &l)
{
    pad(); std::cout << "Load(" << std::endl;
    tab();
    std::visit(*this, l.src_ptr);
    std::visit(*this, l.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Store &s)
{
    pad(); std::cout << "Store(" << std::endl;
    tab();
    std::visit(*this, s.src);
    std::visit(*this, s.dst_ptr);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Jump &j)
{
    pad(); std::cout << "Jump(" << j.target << ")" << std::endl;
}

void TACPrinter::operator()(const tac::JumpIfZero &j)
{
    pad(); std::cout << "JumpIfZero(" << j.target << std::endl;
    tab(); std::visit(*this, j.condition); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::JumpIfNotZero &j)
{
    pad(); std::cout << "JumpIfNotZero(" << j.target << std::endl;
    tab(); std::visit(*this, j.condition); shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Label &j)
{
    pad(); std::cout << "Label(" << j.identifier << ")" << std::endl;
}

void TACPrinter::operator()(const tac::FunctionCall &f)
{
    pad(); std::cout << "FunctionCall(" << f.identifier << std::endl;
    tab();
    for (auto &arg : f.args)
        std::visit(*this, arg);
    if (f.dst)
        std::visit(*this, *f.dst);
    shift_tab();
}

void TACPrinter::operator()(const tac::SignExtend &s)
{
    pad(); std::cout << "SignExtend(" << std::endl;
    tab();
    std::visit(*this, s.src);
    std::visit(*this, s.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Truncate &t)
{
    pad(); std::cout << "Truncate(" << std::endl;
    tab();
    std::visit(*this, t.src);
    std::visit(*this, t.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::ZeroExtend &z)
{
    pad(); std::cout << "ZeroExtend(" << std::endl;
    tab();
    std::visit(*this, z.src);
    std::visit(*this, z.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::DoubleToInt &d)
{
    pad(); std::cout << "DoubleToInt(" << std::endl;
    tab();
    std::visit(*this, d.src);
    std::visit(*this, d.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::DoubleToUInt &d)
{
    pad(); std::cout << "DoubleToUInt(" << std::endl;
    tab();
    std::visit(*this, d.src);
    std::visit(*this, d.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::IntToDouble &i)
{
    pad(); std::cout << "IntToDouble(" << std::endl;
    tab();
    std::visit(*this, i.src);
    std::visit(*this, i.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::UIntToDouble &u)
{
    pad(); std::cout << "UIntToDouble(" << std::endl;
    tab();
    std::visit(*this, u.src);
    std::visit(*this, u.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::AddPtr &a)
{
    pad(); std::cout << "AddPtr(" << std::endl;
    tab();
    std::visit(*this, a.ptr);
    std::visit(*this, a.index);
    pad(); std::cout << "scale = " << a.scale << std::endl;
    std::visit(*this, a.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::CopyToOffset &c)
{
    pad(); std::cout << "CopyToOffset(" << std::endl;
    tab();
    std::visit(*this, c.src);
    pad(); std::cout << "dst_identifier = " << c.dst_identifier << std::endl;
    pad(); std::cout << "offset = " << c.offset << std::endl;
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::CopyFromOffset &c)
{
    pad(); std::cout << "CopyFromOffset(" << std::endl;
    tab();
    pad(); std::cout << "src_identifier = " << c.src_identifier << std::endl;
    pad(); std::cout << "offset = " << c.offset << std::endl;
    std::visit(*this, c.dst);
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::FunctionDefinition &)
{
    assert(false);
}

void TACPrinter::operator()(const tac::StaticVariable &s)
{
    pad();
    std::cout << (s.global ? "global" : "local");
    std::cout << " StaticVariable(" << s.name << ") {" << std::endl;
    tab();
    for (size_t i = 0; i < s.list.size(); i++) {
        pad(); std::cout << toString(s.list[i]) << std::endl;
        if (i == 2) {
            pad(); std::cout << "... " << s.list.size() << " elements" << std::endl;
            break;
        }
    }
    shift_tab();
    pad(); std::cout << "}" << std::endl;
}

void TACPrinter::operator()(const tac::StaticConstant &s)
{
    pad(); std::cout << " StaticVariable(" << s.name << ") {" << std::endl;
    tab();
    pad(); std::cout << toString(s.static_init) << std::endl;
    shift_tab();
    pad(); std::cout << "}" << std::endl;
}

void TACPrinter::PrintFunction(const FunctionDefinition &f)
{
    pad();
    std::cout << (f.global ? "global" : "local");
    std::cout << " Function(" << f.name << ") {" << std::endl;
    tab();
    for (auto &block : f.blocks) {
        if (m_printBlocks) {
            std::cout << "-" << block.id <<  "-----------------prev: ";
            for (auto &pred : block.predecessors)
                std::cout << pred->id << " ";
            std::cout << std::endl;
            tab();
        }
        for (auto &instruction : block.instructions)
            std::visit(*this, instruction);
        if (m_printBlocks) {
            shift_tab();
            std::cout << "--------------------next: ";
            for (auto &successor : block.successors)
                std::cout << successor->id << " ";
            std::cout << std::endl;
        }
    }
    shift_tab();
    pad(); std::cout << "}" << std::endl;
}

void TACPrinter::Print(const std::list<TopLevel> &topLevel, Context *context)
{
    TACPrinter printer(context);
    for (const auto &item : topLevel) {
        std::visit([&](const auto &node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, FunctionDefinition>)
                printer.PrintFunction(node);
            else
                std::visit(printer, item);
        }, item);
    }
}

} // namespace tac
