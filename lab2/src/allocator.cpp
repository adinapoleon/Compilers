#include "allocator.h"
#include <iostream>
#include <climits>

// start address for memory spills
static const int SPILL_BASE_ADDRESS = 32768;

// constructor 
RegisterAllocator::RegisterAllocator(int registerCount) 
    : registerCount(registerCount), nextSpillAddress(SPILL_BASE_ADDRESS) {
    
    // setup physical register tracking
    physicalRegisters.resize(registerCount);
    for (auto& registerState : physicalRegisters) { 
        registerState.allocatedVirtualRegister = -1; // -1 means register is free
        registerState.nextUseDistance = INT_MAX; // no next use yet
    }
}

// pre-pass to compute next use distance for each virtual register
void RegisterAllocator::computeFurthestNextUse(IRNode* instructionList) {
    if (!instructionList) return;

    // find the last instruction to start backward pass
    IRNode* lastInstruction = instructionList;
    while (lastInstruction->next) { 
        lastInstruction = lastInstruction->next;
    }

    // track distance to next use for each virtual register
    std::unordered_map<int, int> nextUseDistance;
    int currentIndex = 0;
    
    // count total instructions to set initial distances
    for (auto* instruction = instructionList; instruction; instruction = instruction->next) { 
        currentIndex++;
    }

    // walk backwards through instructions to update next use distances
    for (auto* instruction = lastInstruction; instruction; instruction = instruction->prev) {
        currentIndex--;
        
        // helper to get current distance or infinity if not found
        auto getNextUseDistance = [&](int virtualRegister) {
            if (virtualRegister < 0) {
                return INT_MAX;
            }
            auto iterator = nextUseDistance.find(virtualRegister);
            return iterator == nextUseDistance.end() ? INT_MAX : iterator->second;
        };

        // update distances based on opcode and register usage
        switch (instruction->opcode) {
            case TOKEN_LOAD:
                // load uses vr1 and defines vr3
                instruction->nu1 = getNextUseDistance(instruction->vr1); 
                instruction->nu3 = getNextUseDistance(instruction->vr3);
                nextUseDistance.erase(instruction->vr3); // definition kills previous next use
                nextUseDistance[instruction->vr1] = currentIndex; // use updates next use
                break;

            case TOKEN_STORE:
                // store uses both vr1 and vr3
                instruction->nu1 = getNextUseDistance(instruction->vr1); 
                instruction->nu3 = getNextUseDistance(instruction->vr3);
                nextUseDistance[instruction->vr1] = currentIndex; 
                nextUseDistance[instruction->vr3] = currentIndex; 
                break;

            case TOKEN_LOADI:
                // loadi defines vr3
                instruction->nu3 = getNextUseDistance(instruction->vr3); 
                nextUseDistance.erase(instruction->vr3); 
                break;

            case TOKEN_ADD: 
            case TOKEN_SUB: 
            case TOKEN_MULT:
            case TOKEN_LSHIFT: 
            case TOKEN_RSHIFT:
                // arithmetic ops use vr1, vr2 and define vr3
                instruction->nu1 = getNextUseDistance(instruction->vr1); 
                instruction->nu2 = getNextUseDistance(instruction->vr2); 
                instruction->nu3 = getNextUseDistance(instruction->vr3);

                nextUseDistance.erase(instruction->vr3);
                nextUseDistance[instruction->vr1] = currentIndex; 
                nextUseDistance[instruction->vr2] = currentIndex;
                break;

            default: 
                break;
        }
    }
}

// returns the index of the reserved scratch register
int RegisterAllocator::getScratchRegisterIndex() const { 
    return registerCount - 1; 
}

// gets memory address for a virtual register that needs to be spilled
// if it hasn't been spilled before, assigns a new unique address
int RegisterAllocator::getOrAssignSpillAddress(int virtualRegister) {
    auto iterator = spillLocationMap.find(virtualRegister);
    if (iterator != spillLocationMap.end()) {
        return iterator->second; // return existing address
    }

    // assign new address and increment for next spill
    spillLocationMap[virtualRegister] = nextSpillAddress; 
    nextSpillAddress += 4;  // each spill uses 4 bytes

    return spillLocationMap[virtualRegister];
}

// marks a physical register as free and removes its virtual mapping
void RegisterAllocator::releasePhysicalRegister(int physicalRegister) {
    // don't release scratch register or out of bounds
    if (physicalRegister < 0 || physicalRegister >= registerCount - 1) {
        return;
    }

    int virtualRegister = physicalRegisters[physicalRegister].allocatedVirtualRegister;
    if (virtualRegister >= 0) {
        virtualToPhysicalMap.erase(virtualRegister);
    }
    physicalRegisters[physicalRegister].allocatedVirtualRegister = -1; 
    physicalRegisters[physicalRegister].nextUseDistance = INT_MAX;
}

// searches for any physical register that is currently unassigned
int RegisterAllocator::findFreePhysicalRegister() {
    for (int registerIndex = 0; registerIndex < registerCount - 1; registerIndex++) {
        if (physicalRegisters[registerIndex].allocatedVirtualRegister == -1) {
            return registerIndex;
        }
    }
    return -1; // no free registers found
}

// finds the register whose next use is furthest in the future
int RegisterAllocator::findRegisterWithFurthestNextUse(int excludedRegister1, int excludedRegister2) {
    int bestRegister = -1;
    int furthestDistance = -1;
    
    for (int registerIndex = 0; registerIndex < registerCount - 1; registerIndex++) {
        // don't pick registers that are currently being used as source operands
        if (registerIndex == excludedRegister1 || registerIndex == excludedRegister2) {
            continue;
        }
        
        int currentDistance = physicalRegisters[registerIndex].nextUseDistance;
        if (currentDistance > furthestDistance) { 
            furthestDistance = currentDistance; 
            bestRegister = registerIndex; 
        }
    }
    return bestRegister;
}

// generates code to save a virtual register from a physical register to memory
void RegisterAllocator::generateSpillCode(int physicalRegister, std::vector<std::string>& outputBuffer) {
    int virtualRegister = physicalRegisters[physicalRegister].allocatedVirtualRegister;
    if (virtualRegister < 0) {
        return; // nothing to spill
    }

    int spillAddress = getOrAssignSpillAddress(virtualRegister);
    int scratchReg = getScratchRegisterIndex();
    
    // emit loadI to get address into scratch, then store register value
    outputBuffer.push_back("loadI " + std::to_string(spillAddress) + " => " + formatRegister(scratchReg));
    outputBuffer.push_back("store " + formatRegister(physicalRegister) + " => " + formatRegister(scratchReg));

    // cleanup mapping
    virtualToPhysicalMap.erase(virtualRegister);
    physicalRegisters[physicalRegister].allocatedVirtualRegister = -1; 
    physicalRegisters[physicalRegister].nextUseDistance = INT_MAX;
}

// generates code to load a spilled virtual register from memory back to a physical register
void RegisterAllocator::generateRestoreCode(int virtualRegister, int physicalRegister, std::vector<std::string>& outputBuffer) {
    auto iterator = spillLocationMap.find(virtualRegister);
    if (iterator == spillLocationMap.end()) {
        return; // was never spilled, so nothing to restore
    }
    
    int scratchReg = getScratchRegisterIndex();
    // emit loadI to get address into scratch, then load value into target register
    outputBuffer.push_back("loadI " + std::to_string(iterator->second) + " => " + formatRegister(scratchReg));
    outputBuffer.push_back("load " + formatRegister(scratchReg) + " => " + formatRegister(physicalRegister));
}

// ensures a source operand (virtual register) is in a physical register
// spills another register if necessary to make room
int RegisterAllocator::prepareSourceOperand(int virtualRegister, int nextUseDistance, int lockedRegister1, int lockedRegister2, std::vector<std::string>& outputBuffer) {
    if (virtualRegister < 0) return -1;
    
    // check if virtual register is already in a physical register
    auto mappingIterator = virtualToPhysicalMap.find(virtualRegister);
    if (mappingIterator != virtualToPhysicalMap.end()) {
        int physicalRegister = mappingIterator->second;
        physicalRegisters[physicalRegister].nextUseDistance = nextUseDistance; // update next use
        return physicalRegister;
    }
    
    // find a free register or spill one if none available
    int physicalRegister = findFreePhysicalRegister();
    if (physicalRegister == -1) {
        // spill the one that won't be used for the longest time
        int victimRegister = findRegisterWithFurthestNextUse(lockedRegister1, lockedRegister2);
        if (physicalRegisters[victimRegister].nextUseDistance == INT_MAX) {
            releasePhysicalRegister(victimRegister); // no need to spill if never used again
        } else {
            generateSpillCode(victimRegister, outputBuffer);
        }
        physicalRegister = victimRegister;
    }
    
    // restore the value from memory if it was spilled
    generateRestoreCode(virtualRegister, physicalRegister, outputBuffer);
    
    // update state
    physicalRegisters[physicalRegister] = {virtualRegister, nextUseDistance}; 
    virtualToPhysicalMap[virtualRegister] = physicalRegister;
    return physicalRegister;
}

// similar to prepareSourceOperand but can use the scratch register as a last resort
int RegisterAllocator::prepareSourceOperandWithScratch(int virtualRegister, int nextUseDistance, int lockedRegister, std::vector<std::string>& outputBuffer) {
    if (virtualRegister < 0) return -1;
    
    // check if already in a permanent register
    auto mappingIterator = virtualToPhysicalMap.find(virtualRegister);
    if (mappingIterator != virtualToPhysicalMap.end()) {
        int physicalRegister = mappingIterator->second;
        physicalRegisters[physicalRegister].nextUseDistance = nextUseDistance;
        return physicalRegister;
    }
    
    // try to allocate a permanent register first
    int physicalRegister = findFreePhysicalRegister();
    if (physicalRegister != -1) {
        generateRestoreCode(virtualRegister, physicalRegister, outputBuffer);
        physicalRegisters[physicalRegister] = {virtualRegister, nextUseDistance}; 
        virtualToPhysicalMap[virtualRegister] = physicalRegister;
        return physicalRegister;
    }
    
    // spill if possible
    int victimRegister = findRegisterWithFurthestNextUse(lockedRegister, -1);
    if (victimRegister != -1) {
        if (physicalRegisters[victimRegister].nextUseDistance == INT_MAX) {
            releasePhysicalRegister(victimRegister);
        } else {
            generateSpillCode(victimRegister, outputBuffer);
        }
        physicalRegister = victimRegister;
        generateRestoreCode(virtualRegister, physicalRegister, outputBuffer);
        physicalRegisters[physicalRegister] = {virtualRegister, nextUseDistance}; 
        virtualToPhysicalMap[virtualRegister] = physicalRegister;
        return physicalRegister;
    }
    
    // last resort: use scratch register (won't be mapped permanently)
    physicalRegister = getScratchRegisterIndex();
    generateRestoreCode(virtualRegister, physicalRegister, outputBuffer);
    physicalRegisters[physicalRegister] = {virtualRegister, nextUseDistance};
    return physicalRegister;
}

// finds a physical register for a destination operand
int RegisterAllocator::prepareDestinationOperand(int lockedRegister1, int lockedRegister2,
                                                 std::vector<std::string>& outputBuffer) {
    int physicalRegister = findFreePhysicalRegister();
    if (physicalRegister != -1) return physicalRegister;
    
    // if no free registers, find one to spill
    int victimRegister = findRegisterWithFurthestNextUse(lockedRegister1, lockedRegister2);
    if (victimRegister == -1) {
        // safety fallback if somehow all are locked
        for (int registerIndex = 0; registerIndex < registerCount - 1; registerIndex++) {
            if (physicalRegisters[registerIndex].nextUseDistance == INT_MAX) { 
                releasePhysicalRegister(registerIndex); 
                return registerIndex; 
            }
        }
        victimRegister = (lockedRegister1 >= 0) ? (lockedRegister1 == 0 ? 1 : 0) : 0;
    }
    
    if (physicalRegisters[victimRegister].nextUseDistance == INT_MAX) {
        releasePhysicalRegister(victimRegister); // just free it if no future use
    } else {
        generateSpillCode(victimRegister, outputBuffer); // must save to memory
    }
    return victimRegister;
}

// main entry point for register allocation
// performs a single pass over the instructions and assigns physical registers
void RegisterAllocator::allocateRegisters(IRNode* instructionList) {
    if (!instructionList) return;

    // compute next use distances first to inform spill decisions
    computeFurthestNextUse(instructionList);

    // iterate through each instruction in the IR
    for (auto* instruction = instructionList; instruction; instruction = instruction->next) {
        std::vector<std::string> preInstructionBuffer; // holds spill/restore code
        int sourceReg1 = -1, sourceReg2 = -1, destReg = -1;

        switch (instruction->opcode) {
            case TOKEN_LOAD: {
                // prepare source operand (memory address)
                sourceReg1 = prepareSourceOperand(instruction->vr1, instruction->nu1, -1, -1, preInstructionBuffer);

                // if dest is same as source, we can reuse the register
                if (instruction->vr3 == instruction->vr1) {
                    destReg = sourceReg1;
                } else {
                    // free source if it has no future use
                    if (instruction->nu1 == INT_MAX) releasePhysicalRegister(sourceReg1);
                    destReg = prepareDestinationOperand(sourceReg1, -1, preInstructionBuffer);
                }

                // update mappings for destination virtual register
                int previousVirtualReg = physicalRegisters[destReg].allocatedVirtualRegister;
                if (previousVirtualReg >= 0 && previousVirtualReg != instruction->vr3) {
                    virtualToPhysicalMap.erase(previousVirtualReg);
                }

                physicalRegisters[destReg] = {instruction->vr3, instruction->nu3}; 
                virtualToPhysicalMap[instruction->vr3] = destReg;

                // output all generated spill/restore instructions before the main op
                for (auto& preInstruction : preInstructionBuffer) emitInstruction(preInstruction);
                emitInstruction("load " + formatRegister(sourceReg1) + " => " + formatRegister(destReg));

                // cleanup if source not reused
                if (destReg != sourceReg1 && instruction->nu1 == INT_MAX) releasePhysicalRegister(sourceReg1);
                break;
            }

            case TOKEN_LOADI: {
                // loadi only has a destination
                destReg = prepareDestinationOperand(-1, -1, preInstructionBuffer);

                int previousVirtualReg = physicalRegisters[destReg].allocatedVirtualRegister;
                if (previousVirtualReg >= 0) virtualToPhysicalMap.erase(previousVirtualReg);

                physicalRegisters[destReg] = {instruction->vr3, instruction->nu3}; 
                virtualToPhysicalMap[instruction->vr3] = destReg;

                for (auto& preInstruction : preInstructionBuffer) emitInstruction(preInstruction);
                emitInstruction("loadI " + std::to_string(instruction->sr1) + " => " + formatRegister(destReg));
                break;
            }

            case TOKEN_STORE: {
                // store uses two source registers (value and address)
                sourceReg1 = prepareSourceOperand(instruction->vr1, instruction->nu1, -1, -1, preInstructionBuffer);
                sourceReg2 = prepareSourceOperand(instruction->vr3, instruction->nu3, sourceReg1, -1, preInstructionBuffer);

                for (auto& preInstruction : preInstructionBuffer) emitInstruction(preInstruction);
                emitInstruction("store " + formatRegister(sourceReg1) + " => " + formatRegister(sourceReg2));

                // free registers if no future uses
                if (instruction->nu1 == INT_MAX) releasePhysicalRegister(sourceReg1);
                if (instruction->nu3 == INT_MAX) releasePhysicalRegister(sourceReg2);
                break;
            }

            case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT:
            case TOKEN_LSHIFT: case TOKEN_RSHIFT: {
                // arithmetic ops: check if we can reuse source registers for destination
                bool reuseSource1ForDest = (instruction->vr3 == instruction->vr1);
                bool reuseSource2ForDest = (instruction->vr3 == instruction->vr2);

                // get physical registers for source operands
                sourceReg1 = prepareSourceOperand(instruction->vr1, instruction->nu1, -1, -1, preInstructionBuffer);
                // second operand might use scratch if we're out of registers
                sourceReg2 = prepareSourceOperandWithScratch(instruction->vr2, instruction->nu2, sourceReg1, preInstructionBuffer);

                if (reuseSource1ForDest) {
                    destReg = sourceReg1;
                } else if (reuseSource2ForDest) {
                    destReg = sourceReg2;
                } else {
                    // try to free source registers before allocating destination
                    if (instruction->nu1 == INT_MAX) releasePhysicalRegister(sourceReg1);
                    if (instruction->nu2 == INT_MAX && sourceReg2 != getScratchRegisterIndex()) releasePhysicalRegister(sourceReg2);

                    // allocate destination, avoiding registers currently holding sources
                    if (sourceReg2 == getScratchRegisterIndex()) {
                        destReg = prepareDestinationOperand(sourceReg1, -1, preInstructionBuffer);
                    } else {
                        destReg = prepareDestinationOperand(sourceReg1, sourceReg2, preInstructionBuffer);
                    }
                }

                // update destination mapping
                int previousVirtualReg = physicalRegisters[destReg].allocatedVirtualRegister;
                if (previousVirtualReg >= 0 && previousVirtualReg != instruction->vr3) {
                    virtualToPhysicalMap.erase(previousVirtualReg);
                }

                physicalRegisters[destReg] = {instruction->vr3, instruction->nu3}; 
                virtualToPhysicalMap[instruction->vr3] = destReg;

                for (auto& preInstruction : preInstructionBuffer) emitInstruction(preInstruction);

                // determine string opcode
                std::string operation;
                switch (instruction->opcode) {
                    case TOKEN_ADD: operation="add"; break; 
                    case TOKEN_SUB: operation="sub"; break;
                    case TOKEN_MULT: operation="mult"; break; 
                    case TOKEN_LSHIFT: operation="lshift"; break;
                    case TOKEN_RSHIFT: operation="rshift"; break; 
                    default: break;
                }

                emitInstruction(operation + " " + formatRegister(sourceReg1) + ", " + formatRegister(sourceReg2) + " => " + formatRegister(destReg));

                // special handling if we used the scratch register for an operand
                int scratchReg = getScratchRegisterIndex();
                if (sourceReg2 == scratchReg) {
                    if (physicalRegisters[scratchReg].allocatedVirtualRegister >= 0) {
                        virtualToPhysicalMap.erase(physicalRegisters[scratchReg].allocatedVirtualRegister);
                    }
                    physicalRegisters[scratchReg].allocatedVirtualRegister = -1; 
                    physicalRegisters[scratchReg].nextUseDistance = INT_MAX;
                }

                // final cleanup for registers with no future uses
                if (destReg != sourceReg1 && instruction->nu1 == INT_MAX) releasePhysicalRegister(sourceReg1);
                if (destReg != sourceReg2 && sourceReg2 != scratchReg && instruction->nu2 == INT_MAX) {
                    releasePhysicalRegister(sourceReg2);
                }
                break;
            }

            case TOKEN_OUTPUT: 
                // output just prints a constant, no registers involved
                emitInstruction("output " + std::to_string(instruction->sr1)); 
                break;

            case TOKEN_NOP: 
                // ignore nops
                break;

            default: 
                // unknown instruction
                break;
        }
    }
}