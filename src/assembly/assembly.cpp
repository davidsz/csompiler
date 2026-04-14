#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "common/context.h"
#include "constant_map.h"
#include "interference_graph.h"

namespace assembly {

// postprocess.cpp
void postprocessPseudoRegisters(
    std::list<TopLevel> &asm_list,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table);
void postprocessInvalidInstructions(
    std::list<TopLevel> &asm_list);

std::string from_tac(
    const std::list<tac::TopLevel> &tac_list,
    Context *context)
{
    // Use one constant dictionary across all ASMBuilders
    std::shared_ptr<ConstantMap> constants = std::make_shared<ConstantMap>();

    std::list<TopLevel> asm_list;
    ASMBuilder tac_to_asm(context, constants);
    tac_to_asm.ConvertTopLevel(tac_list, asm_list);

    context->asmSymbolTable->InsertSymbols(context);
    context->asmSymbolTable->InsertConstants(constants);

    // Register allocation
    // TODO: Build interference graph
    // TODO: Calculate spill costs
    // TODO: Color graph
    // TODO: Create register map and apply it in postprocessPseudoRegisters()

    postprocessPseudoRegisters(asm_list, context->asmSymbolTable);

    postprocessInvalidInstructions(asm_list);

    ASMPrinter asm_printer(context, context->asmSymbolTable);
    return asm_printer.ToText(asm_list);
}

}; // assembly
