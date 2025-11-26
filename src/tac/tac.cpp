#include "tac.h"
#include "tac_builder.h"

namespace tac {

std::vector<TopLevel> from_ast(const std::vector<parser::Declaration> &ast_root)
{
    tac::TACBuilder astToTac;
    return astToTac.ConvertTopLevel(ast_root);
}

} // namespace tac
