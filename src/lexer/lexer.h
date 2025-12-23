#pragma once

#include <list>
#include <string>

namespace lexer {

class Token;

struct Result
{
    std::list<Token> tokens;
    std::string error_message;
    int return_code = 0;
};

Result tokenize(std::string_view code);

}; // lexer
