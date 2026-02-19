#include "cli2.h"
#include <iostream>
#include <stdexcept>

void print_help() {
	std::cout << "Usage: 434alloc [option] <name>" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -h        	Print this help message" << std::endl;
	std::cout << "  -x <name> 	Scan, parse, and print renamed ILOC block" << std::endl;
	std::cout << "  <k> <name> 	Allocate registers using k registers (3 <= k <= 64)" << std::endl;
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions result;
    result.valid = true;
	result.k = 0;
    
	// -h flag
    if (argc == 2 && std::string(argv[1]) == "-h") {
		result.mode = MODE_HELP;
		return result;
	}

	// -x flag
	if (argc == 3 && std::string(argv[1]) == "-x") {
		result.mode = MODE_RENAME;
		result.filename = std::string(argv[2]);
		return result;
	}

	// k flag
	if (argc == 3) {
		try {
			result.k = std::stoi(argv[1]);
		} catch (std::exception&) {
			result.valid = false;
			result.errorMessage = "Invalid argument: '" + std::string(argv[1]) + "' is not a valid register count or flag.";
			return result;
		}

		if (result.k < 3 || result.k > 64) {
			result.valid = false;
			result.errorMessage = "Invalid register count: k must be between 3 and 64, got " + std::string(argv[1]);
			return result;
		}

		result.mode = MODE_ALLOC;
		result.filename = std::string(argv[2]);
		return result;
	}

	result.valid = false;
	result.errorMessage = "Usage: 434alloc -h | 434alloc -x <file> | 434alloc k <file>";
	return result;
}