#include "assembly.h"
#include "asm_builder.h"
#include "asm_printer.h"
#include "postprocess.h"

namespace assembly {

std::string from_tac(std::vector<tac::TopLevel> tacVector)
{
    ASMBuilder tacToAsm;
    std::list<TopLevel> asmList = tacToAsm.ConvertTopLevel(tacVector);

    postprocessStackVariables(asmList);
    postprocessInvalidInstructions(asmList);

    ASMPrinter asmPrinter;
    return asmPrinter.ToText(asmList);
}

}; // assembly
