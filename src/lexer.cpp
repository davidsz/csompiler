#include "lexer.h"
#include "token.h"
#include "tokenizer.h"

namespace lexer {

std::list<Token> tokenize(std::string_view source_code)
{
    std::list<Token> ret;

    Tokenizer tokenizer(source_code);
    while (!tokenizer.ReachedEOF()) {
        ret.push_back(tokenizer.NextToken());
    }

    return ret;
}

};
