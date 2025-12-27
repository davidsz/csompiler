#include "labeling.h"
#include <format>

static size_t s_counter = 0;

std::string MakeNameUnique(std::string_view name)
{
    return std::format("{}.{}", name, s_counter++);
}

std::string GenerateTempVariableName()
{
    return std::format("tmp.{}", s_counter++);
}
