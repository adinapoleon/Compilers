#pragma once

#include "parser.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <climits>
#include <iostream>

struct PhysReg {
    int vr;        // Virtual register currently stored (-1 = free)
    int nextUse;   // Next-use distance (INT_MAX = dead)
};

class Allocator {
public:
    explicit Allocator(int k);

    // Perform register allocation and emit final code
    void allocate(IRNode* head);

private:
    int k;                              // Number of physical registers
    std::vector<PhysReg> pr;             // Physical register file
    std::unordered_map<int,int> vrToPR;  // VR -> PR mapping
    std::unordered_map<int,int> mem;     // VR -> spill memory address
    int nextMem;                         // Next available spill address
    int scratchPR;

    // ===== Analysis =====
    void computeNextUse(IRNode* head);

    // ===== Register Management =====
    int  findFree();                     // Find free PR
    void freeSlot(int p);                // Free PR slot
    int  farthest(int ex1, int ex2);     // Farthest next-use victim

    // ===== Spilling =====
    int  memAddr(int vr);                // Get spill memory address
    void doSpill(int victim,
                 int addrPR,
                 std::vector<std::string>& out);

    void doRestore(int vr,
                   int p,
                   std::vector<std::string>& out);

    // ===== Allocation Helpers =====
    int ensure(int vr,
               int nu,
               int lock1,
               int lock2,
               std::vector<std::string>& out);

    int allocDest(int lock1,
                  int lock2,
                  std::vector<std::string>& out);

    // ===== Output Helpers =====
    std::string R(int p) const {
        return "r" + std::to_string(p);
    }

    void out(const std::string& s) const {
        std::cout << s << "\n";
    }
};