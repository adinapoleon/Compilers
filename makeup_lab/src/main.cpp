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

// Print the IR as-is using SR fields (for -p mode, no optimization).
static void printRawIR(IRNode* head) {
    for (IRNode* n = head; n; n = n->next) {
        switch (n->opcode) {
            case TOKEN_LOADI:
                std::cout << "loadI " << n->sr1 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_LOAD:
                std::cout << "load r" << n->sr1 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_STORE:
                std::cout << "store r" << n->sr1 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_ADD:
                std::cout << "add r" << n->sr1 << ", r" << n->sr2 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_SUB:
                std::cout << "sub r" << n->sr1 << ", r" << n->sr2 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_MULT:
                std::cout << "mult r" << n->sr1 << ", r" << n->sr2 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_LSHIFT:
                std::cout << "lshift r" << n->sr1 << ", r" << n->sr2 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_RSHIFT:
                std::cout << "rshift r" << n->sr1 << ", r" << n->sr2 << " => r" << n->sr3 << "\n";
                break;
            case TOKEN_OUTPUT:
                std::cout << "output " << n->sr1 << "\n";
                break;
            case TOKEN_NOP:
                std::cout << "nop\n";
                break;
            default:
                break;
        }
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
            printRawIR(ir);
        }

        freeIR(ir);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}