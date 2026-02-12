#pragma once

#include <string>
#include <vector>

enum Mode {
    MODE_HELP,
    MODE_SCAN,
    MODE_PARSE,
    MODE_PRINT_IR
};

struct CLIOptions {
    Mode mode;
    std::string filename;
    bool valid;
    std::string errorMessage;
};

CLIOptions parse_arguments(int argc, char* argv[]);
void print_help();

