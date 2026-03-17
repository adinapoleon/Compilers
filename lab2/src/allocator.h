#pragma once
#include "parser.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <climits>

struct PhysReg { int vr; int nextUse; };

class Allocator {
public:
    explicit Allocator(int k);
    void allocate(IRNode* head);
private:
    int k;
    std::vector<PhysReg> pr;
    std::unordered_map<int,int> vrToPR;
    std::unordered_map<int,int> mem;
    int nextMem;

    int  sc();   // scratch register = k-1
    void computeNextUse(IRNode* head);
    int  memAddr(int vr);
    void freeSlot(int p);
    int  findFree();
    int  farthest(int ex1, int ex2);
    void doSpill(int p, std::vector<std::string>& o);
    void doRestore(int vr, int p, std::vector<std::string>& o);
    int  ensure(int vr, int nu, int lock1, int lock2, std::vector<std::string>& o);
    int  ensureAllowScratch(int vr, int nu, int lock1, std::vector<std::string>& o);
    int  allocDest(int lock1, int lock2, std::vector<std::string>& o);

    std::string R(int p)                  { return "r" + std::to_string(p); }
    void        out(const std::string& s) { std::cout << s << "\n"; }
};