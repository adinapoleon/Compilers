#include "allocator.h"
#include <iostream>
#include <climits>

// Bottom-up local register allocator.
//
// r(k-1) is the dedicated spill-address scratch register.
// It is never permanently assigned a VR. Spill/restore sequences use it for addresses.
//
// For arithmetic with 2 sources and a new dest, the worst case needs 3 VR slots.
// With k-1 permanent VR slots this is fine for k>=4. For k==3 we have only 2 permanent
// VR slots, but we handle this by allowing the scratch r(k-1) to temporarily hold
// a source operand for one instruction (it's freed immediately after the instruction).

static const int SPILL_BASE = 32768;

Allocator::Allocator(int k) : k(k), nextMem(SPILL_BASE) {
    pr.resize(k);
    for (auto& r : pr) { r.vr = -1; r.nextUse = INT_MAX; }
}

// ============================================================
// Backward pass
// ============================================================
void Allocator::computeNextUse(IRNode* head) {
    if (!head) return;
    IRNode* tail = head;
    while (tail->next) tail = tail->next;
    std::unordered_map<int,int> dist;
    int idx = 0;
    for (auto* x = head; x; x = x->next) idx++;
    for (auto* nd = tail; nd; nd = nd->prev) {
        idx--;
        auto D = [&](int vr) {
            if (vr < 0) return INT_MAX;
            auto it = dist.find(vr);
            return it == dist.end() ? INT_MAX : it->second;
        };
        switch (nd->opcode) {
        case TOKEN_LOAD:
            nd->nu1 = D(nd->vr1); nd->nu3 = D(nd->vr3);
            dist.erase(nd->vr3); dist[nd->vr1] = idx; break;
        case TOKEN_STORE:
            nd->nu1 = D(nd->vr1); nd->nu3 = D(nd->vr3);
            dist[nd->vr1] = idx; dist[nd->vr3] = idx; break;
        case TOKEN_LOADI:
            nd->nu3 = D(nd->vr3); dist.erase(nd->vr3); break;
        case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT:
        case TOKEN_LSHIFT: case TOKEN_RSHIFT:
            nd->nu1 = D(nd->vr1); nd->nu2 = D(nd->vr2); nd->nu3 = D(nd->vr3);
            dist.erase(nd->vr3);
            dist[nd->vr1] = idx; dist[nd->vr2] = idx;
            break;
        default: break;
        }
    }
}

// ============================================================
// Helpers
// ============================================================
int Allocator::sc() { return k - 1; }  // scratch register index

int Allocator::memAddr(int vr) {
    auto it = mem.find(vr);
    if (it != mem.end()) return it->second;
    mem[vr] = nextMem; nextMem += 4;
    return mem[vr];
}

// Free a permanent VR slot (r0..r(k-2))
void Allocator::freeSlot(int p) {
    if (p < 0 || p >= k - 1) return;
    if (pr[p].vr >= 0) vrToPR.erase(pr[p].vr);
    pr[p].vr = -1; pr[p].nextUse = INT_MAX;
}

// Find free slot in r0..r(k-2)
int Allocator::findFree() {
    for (int i = 0; i < k - 1; i++)
        if (pr[i].vr == -1) return i;
    return -1;
}

// Farthest next-use in r0..r(k-2), excluding two indices
int Allocator::farthest(int ex1, int ex2) {
    int best = -1, bd = -1;
    for (int i = 0; i < k - 1; i++) {
        if (i == ex1 || i == ex2) continue;
        if (pr[i].nextUse > bd) { bd = pr[i].nextUse; best = i; }
    }
    return best;
}

void Allocator::doSpill(int p, std::vector<std::string>& o) {
    int vr = pr[p].vr;
    if (vr < 0) return;
    int addr = memAddr(vr);
    o.push_back("loadI " + std::to_string(addr) + " => " + R(sc()));
    o.push_back("store " + R(p) + " => " + R(sc()));
    vrToPR.erase(vr);
    pr[p].vr = -1; pr[p].nextUse = INT_MAX;
}

void Allocator::doRestore(int vr, int p, std::vector<std::string>& o) {
    auto it = mem.find(vr);
    if (it == mem.end()) return;
    o.push_back("loadI " + std::to_string(it->second) + " => " + R(sc()));
    o.push_back("load " + R(sc()) + " => " + R(p));
}

// ============================================================
// ensure: get VR vr into a permanent VR slot (r0..r(k-2))
// ============================================================
int Allocator::ensure(int vr, int nu, int lock1, int lock2,
                      std::vector<std::string>& o) {
    if (vr < 0) return -1;
    auto it = vrToPR.find(vr);
    if (it != vrToPR.end()) {
        pr[it->second].nextUse = nu;
        return it->second;
    }
    int p = findFree();
    if (p == -1) {
        int victim = farthest(lock1, lock2);
        if (pr[victim].nextUse == INT_MAX) freeSlot(victim);
        else                               doSpill(victim, o);
        p = victim;
    }
    doRestore(vr, p, o);
    pr[p] = {vr, nu}; vrToPR[vr] = p;
    return p;
}

// Like ensure but may use the scratch register r(k-1) as a last resort.
// Used for the SECOND source of an arithmetic op when no permanent slot is free.
// The caller must free this slot immediately after the instruction.
int Allocator::ensureAllowScratch(int vr, int nu, int lock1,
                                   std::vector<std::string>& o) {
    if (vr < 0) return -1;
    // Already in a permanent slot?
    auto it = vrToPR.find(vr);
    if (it != vrToPR.end()) {
        pr[it->second].nextUse = nu;
        return it->second;
    }
    // Try a permanent slot first
    int p = findFree();
    if (p != -1) {
        doRestore(vr, p, o);
        pr[p] = {vr, nu}; vrToPR[vr] = p;
        return p;
    }
    // No permanent slot — try spilling something (not lock1)
    int victim = farthest(lock1, -1);
    if (victim != -1) {
        if (pr[victim].nextUse == INT_MAX) freeSlot(victim);
        else                               doSpill(victim, o);
        p = victim;
        doRestore(vr, p, o);
        pr[p] = {vr, nu}; vrToPR[vr] = p;
        return p;
    }
    // All permanent slots locked (only possible at k==3 with both locked).
    // Use scratch register r(k-1) temporarily.
    p = sc();
    // Don't track this VR in vrToPR permanently since it's just for this instruction
    doRestore(vr, p, o);
    pr[p] = {vr, nu};
    // Mark as a temporary slot: don't add to vrToPR (will be cleared after instruction)
    return p;
}

// ============================================================
// allocDest
// ============================================================
int Allocator::allocDest(int lock1, int lock2, std::vector<std::string>& o) {
    int p = findFree();
    if (p != -1) return p;
    int victim = farthest(lock1, lock2);
    if (victim == -1) {
        // Shouldn't happen if ensureFreeSlot was called, but handle gracefully
        // Try using a locked register if it's dead
        for (int i = 0; i < k - 1; i++) {
            if (pr[i].nextUse == INT_MAX) { freeSlot(i); return i; }
        }
        // Last resort
        victim = (lock1 >= 0) ? (lock1 == 0 ? 1 : 0) : 0;
    }
    if (pr[victim].nextUse == INT_MAX) freeSlot(victim);
    else                               doSpill(victim, o);
    return victim;
}

// ============================================================
// Main allocation pass
// ============================================================
void Allocator::allocate(IRNode* head) {
    if (!head) return;
    computeNextUse(head);

    for (auto* nd = head; nd; nd = nd->next) {
        std::vector<std::string> pre;
        int p1=-1, p2=-1, p3=-1;

        switch (nd->opcode) {

        case TOKEN_LOAD: {
            p1 = ensure(nd->vr1, nd->nu1, -1, -1, pre);
            if (nd->vr3 == nd->vr1) {
                p3 = p1;
            } else {
                if (nd->nu1 == INT_MAX) freeSlot(p1);
                p3 = allocDest(p1, -1, pre);
            }
            if (pr[p3].vr >= 0 && pr[p3].vr != nd->vr3) vrToPR.erase(pr[p3].vr);
            pr[p3] = {nd->vr3, nd->nu3}; vrToPR[nd->vr3] = p3;
            for (auto& s : pre) out(s);
            out("load " + R(p1) + " => " + R(p3));
            if (p3 != p1 && nd->nu1 == INT_MAX) freeSlot(p1);
            break;
        }

        case TOKEN_LOADI: {
            p3 = allocDest(-1, -1, pre);
            if (pr[p3].vr >= 0) vrToPR.erase(pr[p3].vr);
            pr[p3] = {nd->vr3, nd->nu3}; vrToPR[nd->vr3] = p3;
            for (auto& s : pre) out(s);
            out("loadI " + std::to_string(nd->sr1) + " => " + R(p3));
            break;
        }

        case TOKEN_STORE: {
            p1 = ensure(nd->vr1, nd->nu1, -1, -1, pre);
            p3 = ensure(nd->vr3, nd->nu3, p1, -1, pre);
            for (auto& s : pre) out(s);
            out("store " + R(p1) + " => " + R(p3));
            if (nd->nu1 == INT_MAX) freeSlot(p1);
            if (nd->nu3 == INT_MAX) freeSlot(p3);
            break;
        }

        case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT:
        case TOKEN_LSHIFT: case TOKEN_RSHIFT: {
            bool reuseP1 = (nd->vr3 == nd->vr1);
            bool reuseP2 = (nd->vr3 == nd->vr2);

            p1 = ensure(nd->vr1, nd->nu1, -1, -1, pre);
            p2 = ensureAllowScratch(nd->vr2, nd->nu2, p1, pre);

            if      (reuseP1) p3 = p1;
            else if (reuseP2) p3 = p2;
            else {
                // Free dead sources before allocating dest
                if (nd->nu1 == INT_MAX) freeSlot(p1);
                if (nd->nu2 == INT_MAX && p2 != sc()) freeSlot(p2);
                // If p2 was in scratch, scratch is implicitly free for dest
                if (p2 == sc()) {
                    // scratch used for p2; p3 must go in a permanent slot
                    p3 = allocDest(p1, -1, pre);
                } else {
                    p3 = allocDest(p1, p2, pre);
                }
            }
            if (pr[p3].vr >= 0 && pr[p3].vr != nd->vr3) vrToPR.erase(pr[p3].vr);
            pr[p3] = {nd->vr3, nd->nu3}; vrToPR[nd->vr3] = p3;

            for (auto& s : pre) out(s);
            std::string op;
            switch (nd->opcode) {
                case TOKEN_ADD: op="add"; break; case TOKEN_SUB: op="sub"; break;
                case TOKEN_MULT: op="mult"; break; case TOKEN_LSHIFT: op="lshift"; break;
                case TOKEN_RSHIFT: op="rshift"; break; default: break;
            }
            out(op + " " + R(p1) + ", " + R(p2) + " => " + R(p3));

            // Clean up scratch if it was used for p2
            if (p2 == sc()) {
                if (pr[sc()].vr >= 0) vrToPR.erase(pr[sc()].vr);
                pr[sc()].vr = -1; pr[sc()].nextUse = INT_MAX;
            }
            if (p3 != p1 && nd->nu1 == INT_MAX) freeSlot(p1);
            if (p3 != p2 && p2 != sc() && nd->nu2 == INT_MAX) freeSlot(p2);
            break;
        }

        case TOKEN_OUTPUT: out("output " + std::to_string(nd->sr1)); break;
        case TOKEN_NOP: break;
        default: break;
        }
    }
}