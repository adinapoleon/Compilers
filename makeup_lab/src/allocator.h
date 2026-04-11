#pragma once

#include "parser.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <climits>
#include <iostream>

// Tracks the state of a physical register
struct PhysicalRegister {
    int allocatedVirtualRegister;   // -1 indicates free register
    int nextUseDistance;    // Distance to next use 
};

class RegisterAllocator {
public:
    explicit RegisterAllocator(int registerCount); //constructor
    
    // main allocate function
    void allocateRegisters(IRNode* instructionList);

private:
    int registerCount;  // physical registers available
    std::vector<PhysicalRegister> physicalRegisters;    // state of each physical register
    std::unordered_map<int, int> virtualToPhysicalMap;  // map virtual register -> physical register
    std::unordered_map<int, int> spillLocationMap;  // map virtual register -> memory spill address
    int nextSpillAddress;   // next address for spills

    // allocation helpers
    int getScratchRegisterIndex() const;    // registerCount - 1
    void computeFurthestNextUse(IRNode* instructionList); 
    int getOrAssignSpillAddress(int virtualRegister);      
    
    // physical register management
    void releasePhysicalRegister(int physicalRegister);
    int findFreePhysicalRegister();
    int findRegisterWithFurthestNextUse(int excludedRegister1, int excludedRegister2);
    
    // spill and restore operations
    void generateSpillCode(int physicalRegister, std::vector<std::string>& outputBuffer);
    void generateRestoreCode(int virtualRegister, int physicalRegister, std::vector<std::string>& outputBuffer);
    
    // operand preparation
    int prepareSourceOperand(int virtualRegister, int nextUseDistance, int lockedRegister1, int lockedRegister2, std::vector<std::string>& outputBuffer);
    int prepareSourceOperandWithScratch(int virtualRegister, int nextUseDistance, int lockedRegister, std::vector<std::string>& outputBuffer);
    int prepareDestinationOperand(int lockedRegister1, int lockedRegister2, std::vector<std::string>& outputBuffer);

    // print helpers
    std::string formatRegister(int registerIndex) { return "r" + std::to_string(registerIndex); }
    void emitInstruction(const std::string& instruction) { std::cout << instruction << "\n"; }
};