#ifndef PARSER_H
#define PARSER_H

#include "ast_nodes.h"
#include "lexer/token.h"
#include <list>

namespace parser {

struct Result
{
    std::unique_ptr<ASTRoot> root;
    std::string error_message;
    int return_code = 0;
};

Result parse(const std::list<lexer::Token> &token_list);

}; // parser

#endif // PARSER_H
