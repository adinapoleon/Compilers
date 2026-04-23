#include "scheduler.h"
#include "renamer.h"
#include <algorithm>
#include <iostream>

// ---------------------------------------------------------------------------
// SchedulerNode
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// DependencyGraph
// ---------------------------------------------------------------------------

DependencyGraph::DependencyGraph() {}

void DependencyGraph::addEdge(SchedulerNode* from, SchedulerNode* to) {
    if (from == to) return;  // never add self-loops
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

    // last writer per VR (for RAW / WAW)
    std::map<int, SchedulerNode*> last_def;
    // all live readers per VR (for WAR)
    std::map<int, std::vector<SchedulerNode*>> last_uses;

    // Memory ordering: single-predecessor chain (O(n) edges, not O(n^2))
    SchedulerNode* last_store  = nullptr;
    SchedulerNode* last_load   = nullptr;
    SchedulerNode* last_output = nullptr;

    while (curr) {
        SchedulerNode* node = new SchedulerNode(curr, count++);
        nodes.push_back(node);

        // Process all USE operands first, then DEF.
        // This order matters for instructions like  add r2, r1 => r2
        // where the same VR appears as both use and def: we must record
        // the use-dependency on the *previous* def before overwriting
        // last_def with this node.

        auto record_use = [&](int reg) {
            if (reg == -1) return;
            if (last_def.count(reg))
                addEdge(last_def[reg], node);           // RAW
            last_uses[reg].push_back(node);
        };

        auto record_def = [&](int reg) {
            if (reg == -1) return;
            if (last_def.count(reg))
                addEdge(last_def[reg], node);           // WAW
            for (auto* use : last_uses[reg])
                addEdge(use, node);                     // WAR
            last_uses[reg].clear();
            last_def[reg] = node;
        };

        switch (curr->opcode) {
            case TOKEN_LOAD:
                record_use(curr->vr1);
                record_def(curr->vr3);
                break;
            case TOKEN_LOADI:
                record_def(curr->vr3);
                break;
            case TOKEN_STORE:
                record_use(curr->vr1);
                record_use(curr->vr3);
                break;
            case TOKEN_ADD:
            case TOKEN_SUB:
            case TOKEN_MULT:
            case TOKEN_LSHIFT:
            case TOKEN_RSHIFT:
                record_use(curr->vr1);
                record_use(curr->vr2);
                record_def(curr->vr3);
                break;
            case TOKEN_OUTPUT:
                // no register operands
                break;
            default:
                break;
        }

        // Memory/output ordering (conservative aliasing assumed)
        if (curr->opcode == TOKEN_STORE) {
            if (last_store)  addEdge(last_store,  node); // store->store WAW
            if (last_load)   addEdge(last_load,   node); // load->store  WAR
            if (last_output) addEdge(last_output, node); // output->store WAR
            last_store = node;
        } else if (curr->opcode == TOKEN_LOAD) {
            if (last_store)  addEdge(last_store,  node); // store->load RAW
            last_load = node;
        } else if (curr->opcode == TOKEN_OUTPUT) {
            if (last_store)  addEdge(last_store,  node); // store->output RAW
            if (last_output) addEdge(last_output, node); // preserve print order
            last_output = node;
        }

        curr = curr->next;
    }
}

// ---------------------------------------------------------------------------
// computePriorities — correct ALAP via true reverse topological sort
// (Kahn's algorithm on the transpose graph)
// ---------------------------------------------------------------------------
void DependencyGraph::computePriorities() {
    int n = nodes.size();

    // remaining[i] = number of children not yet finalized
    std::vector<int> remaining(n, 0);
    for (int i = 0; i < n; i++)
        remaining[i] = (int)nodes[i]->children.size();

    std::vector<SchedulerNode*> q;
    q.reserve(n);
    for (auto* node : nodes) {
        if (node->children.empty()) {
            node->priority = node->latency;
            q.push_back(node);
        }
    }

    size_t head = 0;
    while (head < q.size()) {
        SchedulerNode* node = q[head++];
        for (auto* parent : node->parents) {
            int candidate = node->priority + parent->latency;
            if (candidate > parent->priority)
                parent->priority = candidate;

            remaining[parent->id]--;
            if (remaining[parent->id] == 0)
                q.push_back(parent);
        }
    }
}

// ---------------------------------------------------------------------------
// Scheduler
// ---------------------------------------------------------------------------

Scheduler::Scheduler(DependencyGraph& dg) : graph(dg) {}

bool Scheduler::canFit(IRNode* ir, int unit) {
    if (!ir || ir->opcode == TOKEN_NOP) return true;
    if (unit == 0) {
        return ir->opcode != TOKEN_MULT;
    } else {
        return ir->opcode != TOKEN_LOAD && ir->opcode != TOKEN_STORE;
    }
}

static void printOp(IRNode* node) {
    if (!node || node->opcode == TOKEN_NOP) {
        std::cout << "nop";
        return;
    }
    switch (node->opcode) {
        case TOKEN_LOAD:
            std::cout << "load r"   << node->vr1 << " => r" << node->vr3; break;
        case TOKEN_LOADI:
            std::cout << "loadI "   << node->sr1 << " => r" << node->vr3; break;
        case TOKEN_STORE:
            std::cout << "store r"  << node->vr1 << " => r" << node->vr3; break;
        case TOKEN_ADD:
            std::cout << "add r"    << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_SUB:
            std::cout << "sub r"    << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_MULT:
            std::cout << "mult r"   << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_LSHIFT:
            std::cout << "lshift r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_RSHIFT:
            std::cout << "rshift r" << node->vr1 << ", r" << node->vr2 << " => r" << node->vr3; break;
        case TOKEN_OUTPUT:
            std::cout << "output "  << node->sr1; break;
        default:
            std::cout << "nop"; break;
    }
}

void Scheduler::schedule() {
    std::priority_queue<SchedulerNode*,
                        std::vector<SchedulerNode*>,
                        ComparePriority> ready_q;
    std::vector<SchedulerNode*> active;

    for (auto* node : graph.nodes) {
        if (node->in_degree == 0)
            ready_q.push(node);
    }

    int cycle = 1;
    int scheduled_count = 0;
    int total_nodes = (int)graph.nodes.size();

    while (scheduled_count < total_nodes) {

        // 1. Retire completed ops and release their dependents
        auto it = active.begin();
        while (it != active.end()) {
            if (cycle >= (*it)->ready_at_cycle) {
                for (auto* child : (*it)->children) {
                    child->in_degree--;
                    if (child->in_degree == 0)
                        ready_q.push(child);
                }
                it = active.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Issue: fill f0 then f1
        SchedulerNode* op0 = nullptr;
        SchedulerNode* op1 = nullptr;
        bool output_issued = false;

        std::vector<SchedulerNode*> deferred;

        while (!ready_q.empty() && (!op0 || !op1)) {
            SchedulerNode* node = ready_q.top();
            ready_q.pop();

            if (node->ir->opcode == TOKEN_OUTPUT && output_issued) {
                deferred.push_back(node);
                continue;
            }

            bool issued = false;
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
                deferred.push_back(node);
            }
        }

        for (auto* d : deferred) ready_q.push(d);

        // 3. Print the cycle
        std::cout << "[ ";
        printOp(op0 ? op0->ir : nullptr);
        std::cout << " ; ";
        printOp(op1 ? op1->ir : nullptr);
        std::cout << " ]" << std::endl;

        cycle++;
    }
}