#include "tac.h"
#include "tac_builder.h"

namespace tac {

std::list<TopLevel> from_ast(
    const std::vector<parser::Declaration> &ast_root,
    Context *context)
{
    tac::TACBuilder astToTac(context);
    return astToTac.ConvertTopLevel(ast_root);
}

} // namespace tac
