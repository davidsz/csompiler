#include "tac.h"
#include "tac_builder.h"

namespace tac {

std::vector<Instruction> from_ast(const std::vector<parser::BlockItem> &ast_root)
{
    tac::TACBuilder astToTac;
    return astToTac.Convert(ast_root);
}

} // namespace tac
