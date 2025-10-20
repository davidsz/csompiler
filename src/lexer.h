#ifndef LEXER_H
#define LEXER_H

#include <list>

class Token;

namespace lexer {

struct Result
{
    std::list<Token> tokens;
    std::string error_message;
    int return_code = 0;
};

Result tokenize(std::string_view code);

}; // lexer

#endif // LEXER_H
