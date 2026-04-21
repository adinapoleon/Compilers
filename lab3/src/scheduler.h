#pragma once

#include "parser.h"
#include <vector>
#include <queue>
#include <map>

struct SchedulerNode {
    IRNode* ir;
    int id;
    int priority;
    int latency;
    int in_degree;
    std::vector<SchedulerNode*> children;
    std::vector<SchedulerNode*> parents;

    // Track when this instruction will be completed
    int ready_at_cycle;

    SchedulerNode(IRNode* node, int node_id);
};

class DependencyGraph {
public:
    DependencyGraph();
    void build(IRNode* head);
    void computePriorities();
    std::vector<SchedulerNode*> getRoots();
    std::vector<SchedulerNode*> nodes;

private:
    void addEdge(SchedulerNode* from, SchedulerNode* to);
};

class Scheduler {
public:
    Scheduler(DependencyGraph& dg);
    void schedule();

private:
    DependencyGraph& graph;
    
    struct ComparePriority {
        bool operator()(SchedulerNode* const& n1, SchedulerNode* const& n2) {
            if (n1->priority != n2->priority)
                return n1->priority < n2->priority;
            // Tie breaker: original order (id) or number of children
            return n1->id > n2->id; 
        }
    };

    bool canFit(IRNode* ir, int unit); // unit 0 or 1
};
