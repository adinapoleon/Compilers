#include <iostream>
#include "scanner.h"
#include "parser.h"
#include "cli.h"

int main(int argc, char* argv[]) {
	CLIOptions args = parse_arguments(argc, argv);

	if (!args.valid) {
		std::cerr << args.errorMessage << std::endl;
		print_help();
		return 1;
	}

	switch (args.mode) {
		case MODE_HELP:
			print_help();
			break;

		case MODE_SCAN: {
			Scanner scanner(args.filename);
			scanner.scanAll();
			break;
		}

		case MODE_PARSE: {
			Scanner scanner(args.filename);
			Parser parser(scanner);
			parser.parseAll();
			break;
		}
			

		case MODE_PRINT_IR:
			Scanner scanner(args.filename);
			Parser parser(scanner);
			IRNode* irHead = parser.parseAll();
			if (irHead != nullptr) {
				parser.printIR();
			}
			break;
	}

	return 0;
}