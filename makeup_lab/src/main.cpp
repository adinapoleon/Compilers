#include "scanner.h"
#include "parser.h"
#include "lvn.h"
#include "cli.h"
#include <iostream>

static void freeIR(IRNode* head) {
    while (head) {
        IRNode* next = head->next;
        delete head;
        head = next;
    }
}

int main(int argc, char* argv[]) {
    CLIOptions opts = parse_arguments(argc, argv);

    if (!opts.valid) {
        std::cerr << opts.errorMessage << "\n";
        return 1;
    }

    if (opts.mode == MODE_HELP) {
        print_help();
        return 0;
    }

    try {
        Scanner scanner(opts.filename);
        Parser  parser(scanner);
        IRNode* ir = parser.parseAll();

        if (opts.mode == MODE_OPT) {
            LVN lvn;
            ir = lvn.optimize(ir);
            lvn.printIR(ir);
        } else if (opts.mode == MODE_PARSE_ONLY) {
            parser.printIR();
        }

        freeIR(ir);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}