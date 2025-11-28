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
    void operator()(const Binary &) override;
    void operator()(const Idiv &) override;
    void operator()(const Cdq &) override;
    void operator()(const Cmp &) override;
    void operator()(const Jmp &) override;
    void operator()(const JmpCC &) override;
    void operator()(const SetCC &) override;
    void operator()(const Label &) override;
    void operator()(const Push &) override;
    void operator()(const Call &) override;
    void operator()(const AllocateStack &) override;
    void operator()(const DeallocateStack &) override;
    void operator()(const Function &) override;
    void operator()(std::monostate) override;

    std::string ToText(std::list<TopLevel>);

    std::ostringstream m_codeStream;
};

}; // assembly
