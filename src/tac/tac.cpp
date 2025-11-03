#include "tac.h"
#include "tac_builder.h"

namespace tac {

std::vector<Instruction> from_ast(parser::BlockStatement *ast_root)
{
    tac::TACBuilder astToTac;
    return astToTac.Convert(ast_root);
}

} // namespace tac
