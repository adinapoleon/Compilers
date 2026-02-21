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
    std::unordered_map<int, int> regMap; //maps registers
    int nextNewReg;  //next available register number

    //helper functions
    int getNewRegister(int oldReg);
    void processInstruction(IRNode* node);
    void printInstruction(IRNode* node);

    //track where registers have been seen
    void ensureRegisterMapped(int oldReg);
};