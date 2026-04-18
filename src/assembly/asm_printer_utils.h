#pragma once

#include "asm_nodes.h"

namespace assembly {

std::string getOneByteName(Register reg);
std::string getFourByteName(Register reg);
std::string getEightByteName(Register reg);

}; // assembly
