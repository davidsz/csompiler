#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "asm_printer_utils.h"
#include "common/context.h"
#include "constant_map.h"

namespace assembly {

// register_allocator.cpp
std::map<std::string, Register> allocateRegisters(
    std::list<CFGBlock> &blocks,
    const std::string &function_name,
    ASMSymbolTable *asm_symbol_table);

// postprocess.cpp
void replacePseudoRegisters(
    std::list<CFGBlock> &blocks,
    const std::map<std::string, Register> &register_map,
    std::shared_ptr<ASMSymbolTable> asm_symbol_table);
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

    // Intraprocedural optimization: we work on separate functions
    for (auto &top_level_obj : asm_list) {
        std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Function>) {
                std::map<std::string, Register> register_map =
                    allocateRegisters(obj.blocks, obj.name, context->asmSymbolTable.get());

#if 1
                std::cout << std::endl << "Register map for " << obj.name << ":" << std::endl;
                for (auto &[name, reg] : register_map)
                    std::cout << name << " -> " << getEightByteName(reg) << std::endl;
                std::cout << std::endl;
#endif

                // TODO: Debug
                replacePseudoRegisters(obj.blocks, register_map, context->asmSymbolTable);
            }
        }, top_level_obj);
    }

    // TODO: Rename it or try to merge into replacePseudoRegisters()
    postprocessPseudoRegisters(asm_list, context->asmSymbolTable);

    postprocessInvalidInstructions(asm_list);

    ASMPrinter asm_printer(context, context->asmSymbolTable);
    return asm_printer.ToText(asm_list);
}

}; // assembly
