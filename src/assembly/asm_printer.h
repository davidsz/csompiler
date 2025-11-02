#ifndef ASM_PRINTER_H
#define ASM_PRINTER_H

#include "asm_visitor.h"
#include <cassert>
#include <sstream>

namespace assembly {

struct ASMPrinter : public IASMVisitor<void> {
    void operator()(const Reg &) override;
    void operator()(const Imm &) override;
    void operator()(const Mov &) override;
    void operator()(const Ret &) override;
    void operator()(const Function &) override;
    void operator()(std::monostate) override;

    std::string ToText(std::vector<Instruction>);

    std::ostringstream m_codeStream;
};

}; // assembly

#endif // ASM_PRINTER_H
