#pragma once

#include "asm_nodes.h"

namespace assembly {

template <typename T>
struct IASMVisitor {
    ASM_OPERAND_LIST(ADD_TO_VISITOR)
    ASM_INSTRUCTION_LIST(ADD_TO_VISITOR)
    ASM_TOP_LEVEL_LIST(ADD_TO_VISITOR)
    virtual T operator()(std::monostate) = 0;
};

}; // assembly
