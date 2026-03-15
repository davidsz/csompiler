#include "tac.h"
#include "tac_builder.h"

namespace tac {

void from_ast(
    const std::vector<parser::Declaration> &ast_root,
    std::list<tac::TopLevel> &top_level_out,
    Context *context)
{
    tac::TACBuilder astToTac(context);
    astToTac.ConvertTopLevel(ast_root, top_level_out);
}

} // namespace tac
