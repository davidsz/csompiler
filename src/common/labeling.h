#pragma once

#include <string>

// Generate string identifiers on multiple levels of the compilation
// and ensure that they are not colliding

std::string MakeNameUnique(std::string_view name);
std::string GenerateTempVariableName();
