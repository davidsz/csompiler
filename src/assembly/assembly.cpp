#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "asm_printer_utils.h"
#include "common/context.h"
#include "constant_map.h"

namespace assembly {

// register_allocator.cpp
void allocateRegisters(
    std::list<CFGBlock> &blocks,
    FunEntry *function_entry,
    ASMSymbolTable *asm_symbol_table);

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

#if 1
    // Intraprocedural optimization: we work on separate functions
    for (auto &top_level_obj : asm_list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                ASMSymbolTable *asm_symbol_table = context->asmSymbolTable.get();
                FunEntry *entry = asm_symbol_table->getAs<FunEntry>(obj.name);
                assert(entry);
                if (!entry->defined)
                    return;
                // Determined during register allocation, used in the postprocess step
                entry->callee_saved_registers.clear();
                allocateRegisters(obj.blocks, entry, asm_symbol_table);
            }
        }, top_level_obj);
    }
#endif

    // TODO: Rename it or try to merge into replacePseudoRegisters()
    postprocessPseudoRegisters(asm_list, context->asmSymbolTable);

    postprocessInvalidInstructions(asm_list);

    ASMPrinter asm_printer(context);
    return asm_printer.ToText(asm_list);
}

}; // assembly
