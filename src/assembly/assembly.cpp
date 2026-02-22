#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "asm_symbol_table.h"
#include "constant_map.h"
#include "postprocess.h"

namespace assembly {

std::string from_tac(
    std::vector<tac::TopLevel> tacVector,
    Context *context)
{
    // Use one constant dictionary across all ASMBuilders
    std::shared_ptr<ConstantMap> constants = std::make_shared<ConstantMap>();

    ASMBuilder tacToAsm(context, constants);
    std::list<TopLevel> asmList = tacToAsm.ConvertTopLevel(tacVector);

    std::shared_ptr<ASMSymbolTable> asmSymbolTable =
        std::make_shared<ASMSymbolTable>(context, constants);

    postprocessPseudoRegisters(asmList, asmSymbolTable);
    postprocessInvalidInstructions(asmList);

    ASMPrinter asmPrinter(context, asmSymbolTable);
    return asmPrinter.ToText(asmList);
}

}; // assembly
