#include "allocator.h"
#include <iostream>
#include <climits>

static const int SPILL_BASE = 32768;

Allocator::Allocator(int k) : k(k), nextMem(SPILL_BASE) {
    pr.resize(k);
    for (auto& r : pr) {
        r.vr = -1;
        r.nextUse = INT_MAX;
    }
}

// ============================================================
// Backward pass: compute next-use information
// ============================================================
void Allocator::computeNextUse(IRNode* head) {
    if (!head) return;

    IRNode* tail = head;
    while (tail->next) tail = tail->next;

    std::unordered_map<int,int> nextUse;
    int index = 0;
    for (auto* n = head; n; n = n->next) index++;

    for (auto* n = tail; n; n = n->prev) {
        index--;

        auto NU = [&](int vr) {
            if (vr < 0) return INT_MAX;
            auto it = nextUse.find(vr);
            return (it == nextUse.end()) ? INT_MAX : it->second;
        };

        switch (n->opcode) {

        case TOKEN_LOAD:
            n->nu1 = NU(n->vr1);
            n->nu3 = NU(n->vr3);
            nextUse[n->vr1] = index;
            nextUse.erase(n->vr3);
            break;

        case TOKEN_STORE:
            n->nu1 = NU(n->vr1);
            n->nu3 = NU(n->vr3);
            nextUse[n->vr1] = index;
            nextUse[n->vr3] = index;
            break;

        case TOKEN_LOADI:
            n->nu3 = NU(n->vr3);
            nextUse.erase(n->vr3);
            break;

        case TOKEN_ADD: case TOKEN_SUB:
        case TOKEN_MULT: case TOKEN_LSHIFT: case TOKEN_RSHIFT:
            n->nu1 = NU(n->vr1);
            n->nu2 = NU(n->vr2);
            n->nu3 = NU(n->vr3);
            nextUse[n->vr1] = index;
            nextUse[n->vr2] = index;
            nextUse.erase(n->vr3);
            break;

        default:
            break;
        }
    }
}

// ============================================================
// Helpers
// ============================================================
int Allocator::memAddr(int vr) {
    auto it = mem.find(vr);
    if (it != mem.end()) return it->second;

    mem[vr] = nextMem;
    nextMem += 4;
    return mem[vr];
}

void Allocator::freeSlot(int p) {
    if (pr[p].vr >= 0)
        vrToPR.erase(pr[p].vr);

    pr[p].vr = -1;
    pr[p].nextUse = INT_MAX;
}

int Allocator::findFree() {
    for (int i = 0; i < k; i++)
        if (pr[i].vr == -1)
            return i;
    return -1;
}

int Allocator::farthest(int ex1, int ex2) {
    int best = -1;
    int farNU = -1;

    for (int i = 0; i < k; i++) {
        if (i == ex1 || i == ex2) continue;
        if (pr[i].nextUse > farNU) {
            farNU = pr[i].nextUse;
            best = i;
        }
    }
    return best;
}

// ============================================================
// Spill and Restore
// ============================================================
void Allocator::doSpill(int victim, int addrPR,
                        std::vector<std::string>& out) {

    int vr = pr[victim].vr;
    int addr = memAddr(vr);

    out.push_back("loadI " + std::to_string(addr) +
                  " => " + R(addrPR));
    out.push_back("store " + R(victim) +
                  " => " + R(addrPR));

    freeSlot(victim);
}

void Allocator::doRestore(int vr, int p,
                          std::vector<std::string>& out) {

    auto it = mem.find(vr);
    if (it == mem.end()) return;   // first definition

    int addr = it->second;

    out.push_back("loadI " + std::to_string(addr) +
                  " => " + R(p));
    out.push_back("load " + R(p) +
                  " => " + R(p));
}

// ============================================================
// Ensure VR is in a register
// ============================================================
int Allocator::ensure(int vr, int nu,
                      int lock1, int lock2,
                      std::vector<std::string>& out) {

    if (vr < 0) return -1;

    // Already in register?
    auto it = vrToPR.find(vr);
    if (it != vrToPR.end()) {
        pr[it->second].nextUse = nu;
        return it->second;
    }

    int p = findFree();

    if (p == -1) {
        int victim = farthest(lock1, lock2);

        if (pr[victim].nextUse == INT_MAX) {
            freeSlot(victim);
            p = victim;
        } else {
            int addrPR = findFree();
            if (addrPR == -1) {
                std::cerr << "Allocator error: no scratch register\n";
                exit(1);
            }
            doSpill(victim, addrPR, out);
            p = victim;
        }
    }

    doRestore(vr, p, out);

    pr[p].vr = vr;
    pr[p].nextUse = nu;
    vrToPR[vr] = p;

    return p;
}

// ============================================================
// Allocate destination register
// ============================================================
int Allocator::allocDest(int lock1, int lock2,
                         std::vector<std::string>& out) {

    int p = findFree();
    if (p != -1) return p;

    int victim = farthest(lock1, lock2);

    if (pr[victim].nextUse == INT_MAX) {
        freeSlot(victim);
        return victim;
    }

    int addrPR = findFree();
    if (addrPR == -1) {
        std::cerr << "Allocator error: no scratch register\n";
        exit(1);
    }

    doSpill(victim, addrPR, out);
    return victim;
}

// ============================================================
// Main allocation pass
// ============================================================
void Allocator::allocate(IRNode* head) {
    if (!head) return;

    computeNextUse(head);

    for (auto* n = head; n; n = n->next) {

        std::vector<std::string> pre;
        std::string op;
        int p1=-1, p2=-1, p3=-1;

        switch (n->opcode) {

            case TOKEN_LOAD:
                p1 = ensure(n->vr1, n->nu1, -1, -1, pre);
                if (n->nu1 == INT_MAX) freeSlot(p1);

                p3 = allocDest(p1, -1, pre);
                pr[p3] = { n->vr3, n->nu3 };
                vrToPR[n->vr3] = p3;

                for (auto& s : pre) out(s);
                out("load " + R(p1) + " => " + R(p3));
                break;

            case TOKEN_LOADI:
                p3 = allocDest(-1, -1, pre);
                pr[p3] = { n->vr3, n->nu3 };
                vrToPR[n->vr3] = p3;

                for (auto& s : pre) out(s);
                out("loadI " + std::to_string(n->sr1) +
                    " => " + R(p3));
                break;

            case TOKEN_STORE:
                p1 = ensure(n->vr1, n->nu1, -1, -1, pre);
                p3 = ensure(n->vr3, n->nu3, p1, -1, pre);

                for (auto& s : pre) out(s);
                out("store " + R(p1) +
                    " => " + R(p3));

                if (n->nu1 == INT_MAX) freeSlot(p1);
                if (n->nu3 == INT_MAX) freeSlot(p3);
                break;

            case TOKEN_ADD: case TOKEN_SUB:
            case TOKEN_MULT: case TOKEN_LSHIFT:
            case TOKEN_RSHIFT:

                p1 = ensure(n->vr1, n->nu1, -1, -1, pre);
                p2 = ensure(n->vr2, n->nu2, p1, -1, pre);

                if (n->nu1 == INT_MAX) freeSlot(p1);
                if (n->nu2 == INT_MAX) freeSlot(p2);

                p3 = allocDest(p1, p2, pre);
                pr[p3] = { n->vr3, n->nu3 };
                vrToPR[n->vr3] = p3;

                for (auto& s : pre) out(s);

                switch (n->opcode) {
                    case TOKEN_ADD:    op="add"; break;
                    case TOKEN_SUB:    op="sub"; break;
                    case TOKEN_MULT:   op="mult"; break;
                    case TOKEN_LSHIFT: op="lshift"; break;
                    case TOKEN_RSHIFT: op="rshift"; break;
                    default: break;
                }

                out(op + " " + R(p1) +
                    ", " + R(p2) +
                    " => " + R(p3));
                break;

            case TOKEN_OUTPUT:
                out("output " + std::to_string(n->sr1));
                break;

            case TOKEN_NOP:
                break;

            default:
                break;
        }
    }
}