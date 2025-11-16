#include "tac_printer.h"
#include <iostream>

namespace tac {

void TACPrinter::operator()(const tac::Return &r)
{
    pad(); std::cout << "Return(" << std::endl;
    tab(); std::visit(*this, r.val); shift_tab();
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
    pad(); std::cout << "Constant(" << c.value << ")" << std::endl;
}

void TACPrinter::operator()(const tac::Variant &v)
{
    pad(); std::cout << "Variant(" << v.name << ")" << std::endl;
}

void TACPrinter::operator()(const tac::FunctionDefinition &f)
{
    pad(); std::cout << "Function(" << f.name << ") {" << std::endl;
    tab();
    for (auto &i : f.inst)
        std::visit(*this, i);
    shift_tab();
    pad(); std::cout << "}" << std::endl;
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

void TACPrinter::print(std::vector<Instruction> instructions) {
    for (auto &i : instructions)
        std::visit(*this, i);
}

} // namespace tac
