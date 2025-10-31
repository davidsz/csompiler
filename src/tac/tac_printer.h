#ifndef TAC_PRINTER_H
#define TAC_PRINTER_H

#include "tac_visitor.h"
#include <iostream>

namespace tac {

struct TACPrinter : public ITACVisitor<void> {
    size_t indent = 0;
    void pad() const { std::cout << std::string(indent, ' '); }
    void tab() { indent += 2; }
    void shift_tab() { indent -= 2; }

    void operator()(const tac::Return &r) override {
        std::cout << "Return(";
        std::visit(*this, r.val);
        std::cout << ")" << std::endl;
    }
    void operator()(const tac::Unary &u) override {
        std::cout << "Unary(" << (int)u.op;
        tab();
        std::visit(*this, u.src); std::cout << std::endl;
        std::visit(*this, u.dst); std::cout << std::endl;
        shift_tab();
        std::cout << ")" << std::endl;
    }
    void operator()(const tac::Constant &c) override {
        std::cout << "Constant(" << c.value << ")";
    }
    void operator()(const tac::Variant &v) override {
        std::cout << "Variant(" << v.name << ")";
    }
    void operator()(const tac::FunctionDefinition &f) override {
        std::cout << "Function(" << f.name << ") {" << std::endl;
        tab();
        for (auto &i : f.inst) {
            std::visit(*this, i); std::cout << std::endl;
        }
        shift_tab();
        std::cout << "}" << std::endl;
    }

    void operator()(const tac::Empty &) override {
        assert(false);
    }
};

} // namespace tac

#endif // TAC_PRINTER_H
