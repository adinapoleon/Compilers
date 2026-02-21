#pragma once

#include "parser.h"
#include <unordered_map>
#include <string>
#include <vector>

class RegisterRenamer {
public:
    RegisterRenamer(); //constructor

    //rename registers
    void rename(IRNode* head);

    void printRenamedIR(IRNode* head); // -x flag

    void reset(); //reset renamer state

private:
    std::unordered_map<int, int> regMap; // SR -> current VR
    int nextNewReg;                      // next available VR id

    // helpers
    int getMappedRegister(int oldReg);   // USE
    int assignNewRegister(int oldReg);   // DEF
    void processInstruction(IRNode* node);
    void printInstruction(IRNode* node);
};