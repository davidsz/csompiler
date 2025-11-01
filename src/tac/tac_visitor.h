#ifndef TAC_VISITOR_H
#define TAC_VISITOR_H

#include "tac_nodes.h"

namespace tac {

template <typename T>
struct ITACVisitor {
    TAC_INSTRUCTION_LIST(ADD_TO_VISITOR)
    TAC_VALUE_TYPE_LIST(ADD_TO_VISITOR)
    virtual T operator()(const tac::Empty &) = 0;
};

}; // tac

#endif // TAC_VISITOR_H
