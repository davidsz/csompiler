#include "types.h"
#include <iostream>

bool FunctionType::operator==(const FunctionType &other) const {
    if (params.size() != other.params.size())
        return false;
    for (size_t i = 0; i < params.size(); i++) {
        if (*params[i] != *other.params[i])
            return false;
    }
    return *ret == *other.ret;
}

std::string toString(const ConstantValue &v) {
    return std::visit([](auto x) {
        return std::to_string(x);
    }, v);
}

void DebugPrint(const SymbolTable &symbolTable) {
    for (const auto &[name, entry] : symbolTable) {
        std::cout << name << " ";
        std::cout << (entry.attrs.defined ? "defined" : "undefined") << " ";
        std::cout << (entry.attrs.global ? "global" : "local") << " ";
        if (std::holds_alternative<Initial>(entry.attrs.init)) {
            auto &initial = std::get<Initial>(entry.attrs.init);
            std::cout << "Initial " << *std::get_if<int>(&initial.i);
        } else if (std::holds_alternative<Tentative>(entry.attrs.init))
            std::cout << "Tentative";
        else
            std::cout << "NoInitializer";
        std::cout << std::endl;
    }
}
