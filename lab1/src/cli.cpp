#include "cli.h"
#include <iostream>

void print_help() {
	std::cout << "Usage: 434fe [option] <name>" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -h        Print this help message" << std::endl;
	std::cout << "  -s <name> Scan the input and print tokens" << std::endl;
	std::cout << "  -p <name> Scan and parse the input (default)" << std::endl;
	std::cout << "  -r <name> Scan, parse, and print intermediate representation" << std::endl;
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions result;
    result.mode = MODE_PARSE; //default mode
    result.valid = true;

    std::vector<std::string> flags_found;
    bool has_filename = false;

    //first pass: check for flags and filename
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "-s" || arg == "-p" || arg == "-r") {
            flags_found.push_back(arg);
            
            //check if curr flag requires filename
            if (arg != "-h" && i + 1 < argc && argv[i + 1][0] != '-') {
                result.filename = argv[++i];
                has_filename = true;
            }
        } else if (arg[0] != '-') {
            // no flags, just filename
            if (result.filename.empty()) {
                result.filename = arg;
                has_filename = true;
            } 
        } else {
            result.valid = false;
            result.errorMessage = "Unknown option: " + arg;
            return result;
        }
    }

    //if not flags, but filename provided, default to parse mode
    if (flags_found.empty() && has_filename) {
        flags_found.push_back("-p");
    }

    //check for multiple flags
    if (flags_found.size() > 1) {
        std::cerr << "Warning: Multiple flags provided ("; 
        for (size_t i = 0; i < flags_found.size(); i++) {
            std::cerr << flags_found[i];
            if (i < flags_found.size() - 1) {
                std::cerr << ", ";
            }
        }
        std::cerr << "). Using the highest priority flag." << std::endl;
    }

    //determine mode based on highest priority flag
    bool found_flag = false;
	for (const auto& flag : flags_found) {
		if (flag == "-h") { //help has highest priority
			result.mode = MODE_HELP;
			found_flag = true;
			break;
		}
	}

	if (!found_flag) {
		for (const auto& flag : flags_found) {
			if (flag == "-r") { //print IR has second highest priority
				result.mode = MODE_PRINT_IR;
				found_flag = true;
				break;
			}
		}
	}

	if (!found_flag) {
		for (const auto& flag : flags_found) {
			if (flag == "-p") { //parse has third highest priority
				result.mode = MODE_PARSE;
				found_flag = true;
				break;
			}
		}
	}

	if (!found_flag) {
		for (const auto& flag : flags_found) {
			if (flag == "-s") { //scan has lowest priority
				result.mode = MODE_SCAN;
				found_flag = true;
				break;
			}
		}
	}


    //validate filename presence for modes that need it
    if (result.mode != MODE_HELP && result.filename.empty()) {
        result.valid = false;
        result.errorMessage = "Filename is required for the selected mode.";
        return result;
    }

    return result;
}