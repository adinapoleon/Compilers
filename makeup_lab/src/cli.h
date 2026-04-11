#pragma once

#include <string>

enum Mode {
    MODE_HELP,
    MODE_PARSE_ONLY,   // -p <name>: parse and print unchanged
    MODE_OPT,          // <name>: optimize (LVN) and print
    // Legacy modes kept for Lab2 binary compatibility
    MODE_RENAME,
    MODE_ALLOC,
};

struct CLIOptions {
    Mode mode;
    std::string filename;
    int k;
    bool valid;
    std::string errorMessage;
};

CLIOptions parse_arguments(int argc, char* argv[]);
void print_help();