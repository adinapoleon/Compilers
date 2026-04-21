#include "scheduler.h"
#include "renamer.h"
#include <algorithm>
#include <iostream>
#include <set>

SchedulerNode::SchedulerNode(IRNode* node, int node_id) 
    : ir(node), id(node_id), priority(0), in_degree(0), ready_at_cycle(1) {
    
    switch (node->opcode) {
        case TOKEN_LOAD:
        case TOKEN_STORE:
            latency = 5;
            break;
        case TOKEN_MULT:
            latency = 3;
            break;
        default:
            latency = 1;
            break;
    }
}

DependencyGraph::DependencyGraph() {}

void DependencyGraph::addEdge(SchedulerNode* from, SchedulerNode* to) {
    // Avoid duplicate edges
    for (auto* child : from->children) {
        if (child == to) return;
    }
    from->children.push_back(to);
    to->parents.push_back(from);
    to->in_degree++;
}

void DependencyGraph::build(IRNode* head) {
    int count = 0;
    IRNode* curr = head;
    std::map<int, SchedulerNode*> last_def;
    std::map<int, std::vector<SchedulerNode*>> last_uses;
    
    std::vector<SchedulerNode*> last_mem_ops; // All memory ops to preserve order
    SchedulerNode* last_output = nullptr;

    while (curr) {
        SchedulerNode* node = new SchedulerNode(curr, count++);
        nodes.push_back(node);

        auto check_reg = [&](int reg, bool is_def) {
            if (reg == -1) return;
            if (is_def) {
                if (last_def.count(reg)) addEdge(last_def[reg], node);
                for (auto* use : last_uses[reg]) addEdge(use, node);
                last_uses[reg].clear();
                last_def[reg] = node;
            } else {
                if (last_def.count(reg)) addEdge(last_def[reg], node);
                last_uses[reg].push_back(node);
            }
        };

        switch (curr->opcode) {
            case TOKEN_LOAD:
                check_reg(curr->vr1, false);
                check_reg(curr->vr3, true);
                break;
            case TOKEN_LOADI:
                check_reg(curr->vr3, true);
                break;
            case TOKEN_STORE:
                check_reg(curr->vr1, false);
                check_reg(curr->vr3, false);
                break;
            case TOKEN_ADD:
            case TOKEN_SUB:
            case TOKEN_MULT:
            case TOKEN_LSHIFT:
            case TOKEN_RSHIFT:
                check_reg(curr->vr1, false);
                check_reg(curr->vr2, false);
                check_reg(curr->vr3, true);
                break;
            case TOKEN_OUTPUT:
                // output reads from memory at sr1 (constant)
                break;
            default: break;
        }

        // Memory and Output dependencies
        if (curr->opcode == TOKEN_LOAD || curr->opcode == TOKEN_STORE || curr->opcode == TOKEN_OUTPUT) {
            // For simplicity in this lab, preserve relative order of all memory/output ops
            for (auto* prev : last_mem_ops) {
                addEdge(prev, node);
            }
            last_mem_ops.push_back(node);
            // Limit the size of last_mem_ops if it gets too large, but for basic blocks it should be fine
        }

        curr = curr->next;
    }
}

void DependencyGraph::computePriorities() {
    std::vector<SchedulerNode*> q;
    for (auto* node : nodes) {
        if (node->children.empty()) {
            node->priority = node->latency;
            q.push_back(node);
        }
    }

    int head = 0;
    while(head < q.size()){
        SchedulerNode* node = q[head++];
        for(auto* parent : node->parents){
            int p = node->priority + parent->latency;
            if(p > parent->priority){
                parent->priority = p;
                q.push_back(parent);
            }
        }
    }
}

Scheduler::Scheduler(DependencyGraph& dg) : graph(dg) {}

bool Scheduler::canFit(IRNode* ir, int unit) {
    if (!ir) return true; // NOP fits anywhere
    if (ir->opcode == TOKEN_NOP) return true;
    
    if (unit == 0) {
        // f0: everything except mult
        return ir->opcode != TOKEN_MULT;
    } else {
        // f1: everything except load, store
        return ir->opcode != TOKEN_LOAD && ir->opcode != TOKEN_STORE;
    }
}

static void printOp(IRNode* node) {
    if (!node || node->opcode == TOKEN_NOP) {
        std::cout << "nop";
        return;
    }
    // Simple output format matching the requirement
    switch (node->opcode) {
        case TOKEN_LOAD:   std::cout << "load r" << node->vr1 << " => r" << node->vr3; break;
        case TOKEN_LOADI:  std::cout << "loadI " << node->sr1 << " => r" << node->vr3; break;
        case TOKEN_STORE:  std::cout << "store r" << node->vr1 << " => r" << node->vr3; break;
        case TOKEN_ADD:    std::cout << "add r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_SUB:    std::cout << "sub r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_MULT:   std::cout << "mult r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_LSHIFT: std::cout << "lshift r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_RSHIFT: std::cout << "rshift r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_OUTPUT: std::cout << "output " << node->sr1; break;
        default: std::cout << "nop"; break;
    }
}

void Scheduler::schedule() {
    std::priority_queue<SchedulerNode*, std::vector<SchedulerNode*>, ComparePriority> ready_q;
    std::vector<SchedulerNode*> active;
    
    for (auto* node : graph.nodes) {
        if (node->in_degree == 0) {
            ready_q.push(node);
        }
    }

    int cycle = 1;
    int scheduled_count = 0;
    int total_nodes = graph.nodes.size();

    while (scheduled_count < total_nodes) {
        // 1. Retire nodes
        auto it = active.begin();
        while (it != active.end()) {
            if (cycle >= (*it)->ready_at_cycle) {
                for (auto* child : (*it)->children) {
                    child->in_degree--;
                    if (child->in_degree == 0) {
                        ready_q.push(child);
                    }
                }
                it = active.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Issue up to 2 nodes
        SchedulerNode* op0 = nullptr;
        SchedulerNode* op1 = nullptr;

        // Try to fill f0 and f1
        // Need to be careful about ready_q order and constraints
        std::vector<SchedulerNode*> temp;
        bool output_issued = false;

        while (!ready_q.empty() && (!op0 || !op1)) {
            SchedulerNode* node = ready_q.top();
            ready_q.pop();

            bool issued = false;
            // Check output constraint (only one output per cycle)
            if (node->ir->opcode == TOKEN_OUTPUT && output_issued) {
                temp.push_back(node);
                continue;
            }

            if (!op0 && canFit(node->ir, 0)) {
                op0 = node;
                issued = true;
            } else if (!op1 && canFit(node->ir, 1)) {
                op1 = node;
                issued = true;
            }

            if (issued) {
                if (node->ir->opcode == TOKEN_OUTPUT) output_issued = true;
                node->ready_at_cycle = cycle + node->latency;
                active.push_back(node);
                scheduled_count++;
            } else {
                temp.push_back(node);
            }
        }

        // Put unissued nodes back in ready_q
        for (auto* t : temp) ready_q.push(t);

        // 3. Print cycle
        std::cout << "[ ";
        printOp(op0 ? op0->ir : nullptr);
        std::cout << " ; ";
        printOp(op1 ? op1->ir : nullptr);
        std::cout << " ]" << std::endl;

        cycle++;
    }
}
