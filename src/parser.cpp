#include "parser.h"
#include "ast_builder.h"

namespace parser {

Result parse(const std::list<lexer::Token> &tokens)
{
    Result res;

    ASTBuilder builder(tokens);
    res.root = builder.Build();
    res.return_code = builder.ErrorCode();
    res.error_message = builder.ErrorMessage();

    return res;
}

}; // parser
