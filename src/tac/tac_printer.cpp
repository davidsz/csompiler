#include "tac_printer.h"
#include <iostream>

namespace tac {

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

void TACPrinter::operator()(const tac::Constant &c)
{
    pad(); std::cout << "Constant(" << toString(c.value) << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Variant &v)
{
    pad(); std::cout << "Variant(" << v.name << ")" << std::endl;
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
    pad(); std::cout << "identifier = " << c.dst_identifier << std::endl;
    pad(); std::cout << "offset = " << c.offset << std::endl;
    shift_tab();
    pad(); std::cout << ")" << std::endl;
}

void TACPrinter::operator()(const tac::FunctionDefinition &f)
{
    pad();
    std::cout << (f.global ? "global" : "local");
    std::cout << " Function(" << f.name << ") {" << std::endl;
    tab();
    for (auto &i : f.inst)
        std::visit(*this, i);
    shift_tab();
    pad(); std::cout << "}" << std::endl;
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

void TACPrinter::print(std::vector<TopLevel> instructions) {
    for (auto &i : instructions)
        std::visit(*this, i);
}

} // namespace tac
