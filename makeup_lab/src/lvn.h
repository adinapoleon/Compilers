#pragma once

#include "parser.h"
#include <unordered_map>
#include <string>
#include <optional>

class LVN {
public:
    LVN();

    // Full optimization pipeline: LVN + DCE. Returns (possibly new) head.
    IRNode* optimize(IRNode* head);

    // Print optimized IR (SR fields).
    void printIR(IRNode* head) const;

private:
    int nextVN;

    std::unordered_map<int, int>       regVN;
    std::unordered_map<int, long long> vnConst;
    std::unordered_map<std::string, int> exprVN;
    std::unordered_map<int, int>       vnReg;

    int  newVN();
    int  getVN(int reg);
    void define(int reg, int vn);
    int  canonical(int reg) const;
    std::string exprKey(TokenType op, int vn1, int vn2) const;
    std::optional<long long> fold(TokenType op, long long c1, long long c2) const;

    IRNode* lvnPass(IRNode* head);   // forward LVN pass
    IRNode* dce(IRNode* head);       // backward DCE pass (iterated)

    IRNode* removeNode(IRNode* node, IRNode* head);
    void    printNode(IRNode* node) const;
};