#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "asm_symbol_table.h"
#include "constant_map.h"
#include "postprocess.h"

namespace assembly {

std::string from_tac(
    const std::list<tac::TopLevel> &tac_list,
    Context *context)
{
    // Use one constant dictionary across all ASMBuilders
    std::shared_ptr<ConstantMap> constants = std::make_shared<ConstantMap>();

    std::list<TopLevel> asm_list;
    ASMBuilder tac_to_asm(context, constants);
    tac_to_asm.ConvertTopLevel(tac_list, asm_list);

    std::shared_ptr<ASMSymbolTable> asm_symbol_table =
        std::make_shared<ASMSymbolTable>(context, constants);

    postprocessPseudoRegisters(asm_list, asm_symbol_table);
    postprocessInvalidInstructions(asm_list);

    ASMPrinter asm_printer(context, asm_symbol_table);
    return asm_printer.ToText(asm_list);
}

}; // assembly
