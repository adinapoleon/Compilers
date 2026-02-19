#pragma once

#include <string>
#include <vector>

enum Mode {
    MODE_HELP,
    MODE_RENAME,
    MODE_ALLOC,
};

struct CLIOptions {
    Mode mode;
    std::string filename;
    int k;   //number of registers
    bool valid;
    std::string errorMessage;
};

CLIOptions parse_arguments(int argc, char* argv[]);
void print_help();

