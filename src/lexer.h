#ifndef LEXER_H
#define LEXER_H

#include <list>

class Token;

namespace lexer {

std::list<Token> tokenize(std::string_view code);

}; // lexer

#endif // LEXER_H
