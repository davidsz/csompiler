#include "lexer/lexer.h"
#include "lexer/token.h"
#include "parser/ast_printer.h"
#include "parser/parser.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char **argv)
{
    // Command line arguments
    std::list<std::string> inputs;
    std::list<std::string> flags;
    auto has_flag = [&](const std::string &name) -> bool {
        return std::find(flags.begin(), flags.end(), name) != flags.end();
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) == 0)
            flags.push_back(arg.substr(2)); // --arg
        else
            inputs.push_back(arg); // input arg
    }

    if (inputs.empty()) {
        std::cerr << "Missing input file from arguments. Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    // Reading the input file
    std::ifstream file(inputs.front());
    if (!file) {
        std::cerr << "Could not open the file." << std::endl;
        return 1;
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

    // Lexer
    lexer::Result lexer_result = lexer::tokenize(file_content);
    if (lexer_result.return_code) {
        std::cout << lexer_result.error_message << std::endl;
        return lexer_result.return_code;
    }

#if 1
    for (auto it = lexer_result.tokens.begin(); it != lexer_result.tokens.end(); it++)
        std::cout << *it << std::endl;
#endif

    if (has_flag("lex"))
        return 0;

    // Parser
    parser::Result parser_result = parser::parse(lexer_result.tokens);
    if (parser_result.return_code) {
        std::cout << parser_result.error_message << std::endl;
        return parser_result.return_code;
    }

#if 1
    parser::ASTPrinter printer;
    printer(*(parser_result.root.get()));
    std::cout << "Success!" << std::endl;
#endif

    if (has_flag("parse"))
        return 0;

    // Code generator
    // TODO

    if (has_flag("codegen"))
        return 0;

    // Code emission
    // TODO

    return 0;
}
