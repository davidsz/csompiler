#pragma once

#include "asm_visitor.h"
#include <sstream>

namespace assembly {

struct ASMPrinter : public IASMVisitor<void> {
    void operator()(const Reg &) override;
    void operator()(const Imm &) override;
    void operator()(const Pseudo &) override;
    void operator()(const Stack &) override;
    void operator()(const Mov &) override;
    void operator()(const Ret &) override;
    void operator()(const Unary &) override;
    void operator()(const Function &) override;
    void operator()(std::monostate) override;

    std::string ToText(std::vector<Instruction>);

    std::ostringstream m_codeStream;
};

}; // assembly
