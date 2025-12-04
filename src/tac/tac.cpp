#include "tac.h"
#include "tac_builder.h"

namespace tac {

std::vector<TopLevel> from_ast(
    const std::vector<parser::Declaration> &ast_root,
    std::shared_ptr<SymbolTable> symbolTable)
{
    tac::TACBuilder astToTac(symbolTable);
    return astToTac.ConvertTopLevel(ast_root);
}

} // namespace tac
