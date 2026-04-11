#include "cli.h"
#include <iostream>
#include <string>

void print_help() {
    std::cout << "Usage: 434makeup [option] <file>\n"
              << "\n"
              << "Options:\n"
              << "  -h          Print this help message and exit.\n"
              << "  <file>      Scan, parse, and optimize the ILOC block in <file> using\n"
              << "              Local Value Numbering; print the optimized code to stdout.\n"
              << "  -p <file>   Scan and parse <file>; print the unchanged code to stdout\n"
              << "              (no optimization performed — used to measure optimizer overhead).\n"
              << "\n"
              << "All error messages are written to stderr.\n";
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions result;
    result.valid = true;
    result.k     = 0;

    // -h
    if (argc == 2 && std::string(argv[1]) == "-h") {
        result.mode = MODE_HELP;
        return result;
    }

    // -p <file>
    if (argc == 3 && std::string(argv[1]) == "-p") {
        result.mode     = MODE_PARSE_ONLY;
        result.filename = std::string(argv[2]);
        return result;
    }

    // <file>
    if (argc == 2) {
        std::string arg(argv[1]);
        if (!arg.empty() && arg[0] == '-') {
            result.valid        = false;
            result.errorMessage = "Unknown flag: '" + arg + "'. Run 434makeup -h for usage.";
            return result;
        }
        result.mode     = MODE_OPT;
        result.filename = arg;
        return result;
    }

    result.valid        = false;
    result.errorMessage = "Usage: 434makeup -h | 434makeup <file> | 434makeup -p <file>";
    return result;
}