#pragma once

#include "asm_visitor.h"
#include "asm_symbol_table.h"
#include <sstream>

namespace assembly {

struct ASMPrinter : public IASMVisitor<void> {
    ASMPrinter(std::shared_ptr<ASMSymbolTable> symbolTable);

    void operator()(const Reg &) override;
    void operator()(const Imm &) override;
    void operator()(const Pseudo &) override;
    void operator()(const Stack &) override;
    void operator()(const Data &) override;
    void operator()(const Comment &) override;
    void operator()(const Mov &) override;
    void operator()(const Movsx &) override;
    void operator()(const MovZeroExtend &) override;
    void operator()(const Cvttsd2si &) override;
    void operator()(const Cvtsi2sd &) override;
    void operator()(const Ret &) override;
    void operator()(const Unary &) override;
    void operator()(const Binary &) override;
    void operator()(const Idiv &) override;
    void operator()(const Div &) override;
    void operator()(const Cdq &) override;
    void operator()(const Cmp &) override;
    void operator()(const Jmp &) override;
    void operator()(const JmpCC &) override;
    void operator()(const SetCC &) override;
    void operator()(const Label &) override;
    void operator()(const Push &) override;
    void operator()(const Call &) override;
    void operator()(const Function &) override;
    void operator()(const StaticVariable &) override;
    void operator()(const StaticConstant &) override;
    void operator()(std::monostate) override;

    std::string ToText(std::list<TopLevel>);

    std::ostringstream m_codeStream;
    std::shared_ptr<ASMSymbolTable> m_symbolTable;
};

}; // assembly
