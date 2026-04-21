#include "cli.h"
#include <iostream>
#include <string>

void print_help() {
    std::cout << "Usage: schedule [option] <name>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h        Print this help message" << std::endl;
    std::cout << "  <name>    Scan, parse, and schedule the ILOC block in <name>" << std::endl;
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions result;
    result.valid = true;
    result.mode = MODE_SCHEDULE;

    if (argc < 2) {
        result.valid = false;
        result.errorMessage = "Usage: schedule [option] <name>";
        return result;
    }

    std::string firstArg = argv[1];

    if (firstArg == "-h") {
        result.mode = MODE_HELP;
        return result;
    }

    if (argc == 2) {
        result.filename = firstArg;
        return result;
    }

    result.valid = false;
    result.errorMessage = "Usage: schedule [option] <name>";
    return result;
}
