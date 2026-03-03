#include "scanner.h"
#include "parser.h"
#include "renamer.h"
#include "allocator.h"
#include "cli2.h"
#include <iostream>
#include <cstdlib>

// function to deallocate IR 
void freeIR(IRNode* head) {
    IRNode* current = head;
    while (current != nullptr) {
        IRNode* next = current->next;
        delete current;
        current = next;
    }
}

int main(int argc, char* argv[]) {
    // parse cli arguments
    CLIOptions options = parse_arguments(argc, argv);
    
    if (!options.valid) {
        std::cerr << options.errorMessage << std::endl;
        return 1;
    }
    
    if (options.mode == MODE_HELP) {
        print_help();
        return 0;
    }
    
    try {
        // create scanner
        Scanner scanner(options.filename);
        
        // create parser
        Parser parser(scanner);
        
        // parse the entire file
        IRNode* ir = parser.parseAll();
        
        if (options.mode == MODE_RENAME) {
            //-x - rename and print
            RegisterRenamer renamer;
            renamer.rename(ir);
            renamer.printRenamedIR(ir);
        }
        else if (options.mode == MODE_ALLOC) {
            // rename egisters first
            RegisterRenamer renamer;
            renamer.rename(ir);
            
            // now allocate it
            Allocater alloc(options.k);
            alloc.allocate(ir);
        }
        
        // clean up IR nodes
        freeIR(ir);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}