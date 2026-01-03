#pragma once

#include "tac_visitor.h"

namespace tac {

struct TACPrinter : public ITACVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    void operator()(const tac::Constant &c) override;
    void operator()(const tac::Variant &v) override;
    void operator()(const tac::Return &r) override;
    void operator()(const tac::Unary &u) override;
    void operator()(const tac::Binary &b) override;
    void operator()(const tac::Copy &c) override;
    void operator()(const tac::GetAddress &g) override;
    void operator()(const tac::Load &l) override;
    void operator()(const tac::Store &s) override;
    void operator()(const tac::Jump &j) override;
    void operator()(const tac::JumpIfZero &j) override;
    void operator()(const tac::JumpIfNotZero &j) override;
    void operator()(const tac::Label &j) override;
    void operator()(const tac::FunctionCall &f) override;
    void operator()(const tac::SignExtend &s) override;
    void operator()(const tac::Truncate &t) override;
    void operator()(const tac::ZeroExtend &z) override;
    void operator()(const tac::DoubleToInt &d) override;
    void operator()(const tac::DoubleToUInt &d) override;
    void operator()(const tac::IntToDouble &i) override;
    void operator()(const tac::UIntToDouble &u) override;
    void operator()(const tac::FunctionDefinition &f) override;
    void operator()(const tac::StaticVariable &s) override;
    void operator()(std::monostate) override {
        assert(false);
    }

    void print(std::vector<TopLevel> instructions);
};

} // namespace tac
