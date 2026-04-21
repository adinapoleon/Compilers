#include "parser.h"
#include "renamer.h"
#include "scheduler.h"
#include "cli.h"
#include <iostream>

int main(int argc, char* argv[]) {
    CLIOptions options = parse_arguments(argc, argv);
    if (options.mode == MODE_HELP) {
        print_help();
        return 0;
    }
    if (!options.valid) {
        std::cerr << options.errorMessage << std::endl;
        return 1;
    }

    Scanner scanner(options.filename);
    Parser parser(scanner);
    IRNode* head = parser.parseAll();

    if (!head) {
        return 0;
    }

    // Lab 3 requires register renaming (Lab 2) first
    RegisterRenamer renamer;
    renamer.rename(head);

    // Build Dependency Graph
    DependencyGraph graph;
    graph.build(head);
    graph.computePriorities();

    // Schedule
    Scheduler scheduler(graph);
    scheduler.schedule();

    return 0;
}
