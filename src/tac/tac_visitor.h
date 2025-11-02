#pragma once

#include "tac_nodes.h"

namespace tac {

template <typename T>
struct ITACVisitor {
    TAC_INSTRUCTION_LIST(ADD_TO_VISITOR)
    TAC_VALUE_TYPE_LIST(ADD_TO_VISITOR)
    virtual T operator()(std::monostate) = 0;
};

}; // tac
