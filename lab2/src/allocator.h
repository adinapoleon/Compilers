#pragma once
#include "parser.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <climits>

struct PhysReg {
    int vr;       // VR in this slot (-1 = free)
    int nextUse;  // next-use distance (INT_MAX = never)
};

class Allocator {
public:
    explicit Allocator(int k);
    void allocate(IRNode* head);

private:
    int k;
    std::vector<PhysReg> pr;
    std::unordered_map<int,int> vrToPR;   // vr -> pr index
    std::unordered_map<int,int> mem;      // vr -> spill address
    int nextMem;

    void  computeNextUse(IRNode* head);
    int   memAddr(int vr);
    void  freeSlot(int p);
    int   findFree();

    // Return the PR with farthest nextUse, excluding two PR indices
    int   farthest(int ex1, int ex2);

    // Ensure VR `vr` is in some PR; may emit spills/restores into `out`.
    // `lock1`, `lock2`: PR indices that must NOT be evicted.
    int   ensure(int vr, int nu, int lock1, int lock2, std::vector<std::string>& out);

    // Get a free PR for a destination; must not evict lock1 or lock2.
    int   allocDest(int lock1, int lock2, std::vector<std::string>& out);

    // Spill the value in PR `p` to memory, using `addrPR` as address scratch.
    // addrPR must be free before calling this.
    void  doSpill(int p, int addrPR, std::vector<std::string>& out);

    // Emit restore of VR `vr` into free PR `p`.
    void  doRestore(int vr, int p, std::vector<std::string>& out);

    std::string R(int p)    { return "r" + std::to_string(p); }
    void        out(const std::string& s) { std::cout << s << "\n"; }
};