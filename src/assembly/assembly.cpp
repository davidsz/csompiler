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

    // apply_optimizations(asm_list, context);

    postprocessPseudoRegisters(asm_list, context->asmSymbolTable);

    postprocessInvalidInstructions(asm_list);

    ASMPrinter asm_printer(context, context->asmSymbolTable);
    return asm_printer.ToText(asm_list);
}

void apply_optimizations(std::list<TopLevel> &list, Context *context)
{
    // Intraprocedural optimization: we work on separate functions.
    for (auto &top_level_obj : list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                // Build interference graph
                std::map<GraphKey, GraphData> graph
                    = buildInterferenceGraph(obj.blocks, context->asmSymbolTable);

                // TODO: Color graph
                // TODO: Create register map and apply it in postprocessPseudoRegisters()
            }
        }, top_level_obj);
    }
}

}; // assembly
