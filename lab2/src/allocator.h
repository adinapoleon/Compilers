#pragma once

#include "parser.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <climits>
#include <iostream>

// structure to track physical register state
struct PhysReg { 
    int virtualReg;   // -1 = empty
    int nextUse;   //distance to next use
}; 

class Allocator {
public:
    explicit Allocator(int k); //constructor
    
    // main allocation function
    void allocate(IRNode* head);

private:
    int k;  // number of registers
    std::vector<PhysReg> physRegs;    // physical registers
    std::unordered_map<int, int> virtualToPhysicalMap; // VR to PR mapping
    std::unordered_map<int, int> spillSlots;   // memory spill locations
    int nextSpillAddr; // next available memory address

    // helper functions
    int scratchRegister() const; // get scratch register index (k-1)
    void computeNextUse(IRNode* head);  // compute next use for all VRs
    int memAddr(int virtualReg);    // get or assign memory address for VR
    void freePhysReg(int physicalReg);   // free a physical register slot
    int findFreePhysReg(); // find an empty physical register
    int findRegFarthest(int exclude1, int exclude2); // find register with farthest next use
    
    // spill and restore operations
    void emitSpill(int physicalReg, std::vector<std::string>& output);
    void emitRestore(int virtualReg, int physicalReg, std::vector<std::string>& output);
    
    // register management
    int prepareSourceOperand(int vr, int nu, int lock1, int lock2, std::vector<std::string>& o);
    int prepareScratchSourceOperand(int vr, int nu, int lock1, std::vector<std::string>& o);
    int prepareDestinationOperand(int lock1, int lock2, std::vector<std::string>& o);

    // output helpers
    std::string R(int p) { return "r" + std::to_string(p); }
    void out(const std::string& s) { std::cout << s << "\n"; }
};
