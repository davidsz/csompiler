#include "lexer.h"
#include "token.h"
#include "tokenizer.h"

namespace lexer {

Result tokenize(std::string_view source_code)
{
    Result res;

    Tokenizer tokenizer(source_code);
    while (auto token = tokenizer.NextToken())
        res.tokens.push_back(*token);

    res.return_code = tokenizer.ErrorCode();
    res.error_message = tokenizer.ErrorMessage();
    return res;
}

};
