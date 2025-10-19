#include "src/lexer.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Could not open the file." << std::endl;
        return 1;
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

    std::cout << file_content << std::endl;

    return 0;
}
