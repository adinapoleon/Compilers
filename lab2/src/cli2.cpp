#include "cli2.h"
#include <iostream>

void print_help() {
	std::cout << "Usage: 434alloc [option] <name>" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -h        	Print this help message" << std::endl;
	std::cout << "  -x <name> 	Scan, parse, and print renamed ILOC block" << std::endl;
	std::cout << "  k <name> 	Allocate registers using k registers (3 <= k <= 64)" << std::endl;
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions result;
    result.valid = true;
	result.k = 0;
    
    if (argc == 2 && std::string(argv[1]) == "-h") {
		result.mode = MODE_HELP;
		return result;
	}

	if (argc == 3 && std::string(argv[1]) == "-x") {
		result.mode = MODE_RENAME;
		result.filename = std::string(argv[2]);
		return result;
	}


}