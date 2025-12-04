#include "types.h"
#include <iostream>

void DebugPrint(const SymbolTable &symbolTable) {
    for (const auto &[name, entry] : symbolTable) {
        std::cout << name << " ";
        std::cout << (entry.attrs.defined ? "defined" : "undefined") << " ";
        std::cout << (entry.attrs.global ? "global" : "local") << " ";
        if (std::holds_alternative<Initial>(entry.attrs.init))
            std::cout << "Initial " << std::get<Initial>(entry.attrs.init).i;
        else if (std::holds_alternative<Tentative>(entry.attrs.init))
            std::cout << "Tentative";
        else
            std::cout << "NoInitializer";
        std::cout << std::endl;
    }
}
