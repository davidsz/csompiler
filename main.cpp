#include "assembly/assembly.h"
#include "common/error.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "parser/ast_printer.h"
#include "parser/parser.h"
#include "parser/semantic_analyzer.h"
#include "parser/type_checker.h"
#include "tac/tac.h"
#include "tac/tac_printer.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static void deleteFile(std::filesystem::path file_path)
{
    try {
        if (!std::filesystem::remove(file_path))
            std::cerr << "Couldn't delete file: " << file_path << std::endl;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

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
        else if (arg.rfind("-", 0) == 0)
            flags.push_back(arg.substr(1)); // -arg
        else
            inputs.push_back(arg); // input arg
    }

    if (inputs.empty()) {
        std::cerr << "Missing input file from arguments. Usage: " << argv[0] << " <filename>" << std::endl;
        return Error::DRIVER_ERROR;
    }

    // Preprocessor
    std::filesystem::path output_preprocessed(inputs.front());
    output_preprocessed.replace_extension(".i");
    std::string preproc_command = std::format(
        "gcc -E -P {} -o {}",
        inputs.front(),
        output_preprocessed.string());
    if (std::system(preproc_command.c_str()) != 0) {
        std::cerr << "Can't preprocess with gcc." << std::endl;
        return Error::DRIVER_ERROR;
    }

    // Reading the input file
    std::ifstream file(output_preprocessed);
    if (!file) {
        std::cerr << "Could not open the file." << std::endl;
        return Error::DRIVER_ERROR;
    }
    std::string file_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
    deleteFile(output_preprocessed);

#if 1
    std::cout << "Source code:" << std::endl;
    std::cout << file_content << std::endl;
#endif

    // Lexer
    lexer::Result lexer_result = lexer::tokenize(file_content);
    if (lexer_result.return_code) {
        std::cout << lexer_result.error_message << std::endl;
        return lexer_result.return_code;
    }

#if 0
    for (auto it = lexer_result.tokens.begin(); it != lexer_result.tokens.end(); it++)
        std::cout << *it << std::endl;
#endif

    if (has_flag("lex"))
        return Error::ALL_OK;

    // Parser
    parser::Result parser_result = parser::parse(lexer_result.tokens);
    if (parser_result.return_code) {
        std::cout << parser_result.error_message << std::endl;
        return parser_result.return_code;
    }

#if 1
    std::cout << std::endl << "AST:" << std::endl;
    parser::ASTPrinter astPrinter;
    astPrinter.print(parser_result.root);
#endif

    if (has_flag("parse"))
        return Error::ALL_OK;

    // Semantic analysis
    parser::SemanticAnalyzer semanticAnalyzer;
    if (Error error = semanticAnalyzer.CheckAndMutate(parser_result.root))
        return error;

    parser::TypeChecker typeChecker;
    if (Error error = typeChecker.CheckAndMutate(parser_result.root))
        return error;

#if 1
    std::cout << std::endl << "After semantic analysis:" << std::endl;
    astPrinter.print(parser_result.root);
#endif

    if (has_flag("validate"))
        return Error::ALL_OK;

    // Intermediate representation
    std::vector<tac::Instruction> tacVector = tac::from_ast(parser_result.root);
#if 1
    std::cout << std::endl << "TAC:" << std::endl;
    tac::TACPrinter tacPrinter;
    tacPrinter.print(tacVector);
#endif

    if (has_flag("tacky"))
        return Error::ALL_OK;

    // Assembly generation
    std::string assemblySource = assembly::from_tac(tacVector);
#if 1
    std::cout << std::endl << "ASM:" << std::endl;
    std::cout << assemblySource;
#endif

    if (has_flag("codegen"))
        return Error::ALL_OK;

    // Code emission
    std::filesystem::path output_assembly_path(inputs.front());
    output_assembly_path.replace_extension(".s");
    std::ofstream output_assembly_file(output_assembly_path);
    if (!output_assembly_file)
        throw std::runtime_error("Can't open file: " + output_assembly_path.string());
    output_assembly_file << assemblySource;
    output_assembly_file.close();

    // Compilation
    bool standalone = !has_flag("c");
    std::filesystem::path output_compiled(output_assembly_path);
    if (standalone)
        output_compiled.replace_extension();
    else
        output_compiled.replace_extension(".o");
    std::string compile_command = std::format(
        "gcc {} {} -o {}",
        standalone ? "" : "-c",
        output_assembly_path.string(),
        output_compiled.string());
    if (std::system(compile_command.c_str()) != 0) {
        std::cerr << "Can't compile with gcc." << std::endl;
        return Error::DRIVER_ERROR;
    }
    deleteFile(output_assembly_path);

    return 0;
}
