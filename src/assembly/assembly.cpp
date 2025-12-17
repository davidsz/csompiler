#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "asm_symbol_table.h"
#include "postprocess.h"

namespace assembly {

std::string from_tac(
    std::vector<tac::TopLevel> tacVector,
    std::shared_ptr<SymbolTable> symbolTable)
{
    ASMBuilder tacToAsm(symbolTable);
    std::list<TopLevel> asmList = tacToAsm.ConvertTopLevel(tacVector);

    std::shared_ptr<ASMSymbolTable> asmSymbolTable = CreateASMSymbolTable(symbolTable);

    postprocessPseudoRegisters(asmList, asmSymbolTable);
    postprocessInvalidInstructions(asmList);

    ASMPrinter asmPrinter(asmSymbolTable);
    return asmPrinter.ToText(asmList);
}

}; // assembly
