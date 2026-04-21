#pragma once

#include <string>

enum Mode {
    MODE_HELP,
    MODE_SCHEDULE,
};

struct CLIOptions {
    Mode mode;
    std::string filename;
    bool valid;
    std::string errorMessage;
};

CLIOptions parse_arguments(int argc, char* argv[]);
void print_help();
