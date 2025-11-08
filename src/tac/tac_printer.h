#pragma once

#include "tac_visitor.h"
#include <iostream>

namespace tac {

struct TACPrinter : public ITACVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    void operator()(const tac::Return &r) override {
        pad(); std::cout << "Return(" << std::endl;
        tab(); std::visit(*this, r.val); shift_tab();
        pad(); std::cout << ")" << std::endl;
    }
    void operator()(const tac::Unary &u) override {
        pad(); std::cout << "Unary(" << toString(u.op) << std::endl;
        tab();
        std::visit(*this, u.src);
        std::visit(*this, u.dst);
        shift_tab();
        pad(); std::cout << ")" << std::endl;
    }
    void operator()(const tac::Binary &b) override {
        pad(); std::cout << "Binary(" << toString(b.op) << std::endl;
        tab();
        std::visit(*this, b.src1);
        std::visit(*this, b.src2);
        std::visit(*this, b.dst);
        shift_tab();
        pad(); std::cout << ")" << std::endl;
    }
    void operator()(const tac::Constant &c) override {
        pad(); std::cout << "Constant(" << c.value << ")" << std::endl;
    }
    void operator()(const tac::Variant &v) override {
        pad(); std::cout << "Variant(" << v.name << ")" << std::endl;
    }
    void operator()(const tac::FunctionDefinition &f) override {
        pad(); std::cout << "Function(" << f.name << ") {" << std::endl;
        tab();
        for (auto &i : f.inst)
            std::visit(*this, i);
        shift_tab();
        pad(); std::cout << "}" << std::endl;
    }
    void operator()(std::monostate) override {
        assert(false);
    }

    void print(std::vector<Instruction> instructions) {
        for (auto &i : instructions)
            std::visit(*this, i);
    }
};

} // namespace tac
