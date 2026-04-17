#pragma once

#include "parser.h"
#include <unordered_map>
#include <string>
#include <optional>

class LVN {
public:
    LVN(); //constructor

    // main function: optimize the IR and return new head (may be same as input)
    IRNode* optimize(IRNode* head);

    // Print optimized IR
    void printIR(IRNode* head) const;

private:
    int nextVN; //next available value number

    std::unordered_map<int, int> registerToVN; // map from register to value number
    std::unordered_map<int, long long> vnToConst; // map from value number to constant value
    std::unordered_map<std::string, int> exprToVN; // map from expression key to value number
    std::unordered_map<int, int> vnReg; // map from value number to register

    int  newVN(); // get a new value number
    int  getVN(int reg); // get value number for a register
    void define(int reg, int vn); // define that a register has a value number

    int  canonical(int reg) const; // get canonical value number for a register 
    std::string exprKey(TokenType op, int vn1, int vn2) const; // get a string key for an expression (op, vn1, vn2)
    std::optional<long long> fold(TokenType op, long long c1, long long c2) const; // constant folding for an operation and two constants

    IRNode* lvnPass(IRNode* head);   // forward LVN pass
    IRNode* deadCodeElimination(IRNode* head); // backward DCE pass 

    IRNode* removeNode(IRNode* node, IRNode* head);
    void printNode(IRNode* node) const;
};