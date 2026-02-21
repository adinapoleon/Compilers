#include "scanner.h"
#include "parser.h"
#include "renamer.h"
#include "cli2.h"
#include <iostream>
#include <cstdlib>

// Function to free the IR linked list
void freeIR(IRNode* head) {
    IRNode* current = head;
    while (current != nullptr) {
        IRNode* next = current->next;
        delete current;
        current = next;
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CLIOptions options = parse_arguments(argc, argv);
    
    if (!options.valid) {
        std::cerr << options.errorMessage << std::endl;
        return 1;
    }
    
    if (options.mode == MODE_HELP) {
        print_help();
        return 0;
    }
    
    // For other modes, we need to process a file
    try {
        // Create scanner
        Scanner scanner(options.filename);
        
        // Create parser
        Parser parser(scanner);
        
        // Parse the entire file
        IRNode* ir = parser.parseAll();
        
        if (options.mode == MODE_RENAME) {
            // For -x flag: rename and print
            RegisterRenamer renamer;
            renamer.rename(ir);
            renamer.printRenamedIR(ir);
        }
        else if (options.mode == MODE_ALLOC) {
            // For allocation mode (will be implemented later)
            // First rename the registers
            RegisterRenamer renamer;
            renamer.rename(ir);
            
            // TODO: Implement register allocation
            // For now, just print the renamed code (so we can test)
            // This will be replaced with actual allocation logic
            std::cerr << "Allocation mode with " << options.k 
                     << " registers (using renamed code temporarily)" << std::endl;
            renamer.printRenamedIR(ir);
        }
        
        // Clean up IR nodes
        freeIR(ir);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}