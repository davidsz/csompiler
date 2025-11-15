#pragma once

enum Error {
    ALL_OK = 0,
    DRIVER_ERROR = 1,
    LEXER_ERROR = 2,
    PARSER_ERROR = 3,
    SEMANTIC_ERROR = 4,
};
