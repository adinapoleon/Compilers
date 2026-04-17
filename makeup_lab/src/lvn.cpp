#include "lvn.h"
#include <iostream>
#include <algorithm>
#include <unordered_set>


LVN::LVN() : nextVN(0) {}  //constructor

// new value number
int LVN::newVN() { return nextVN++; }

// get value number for a register, assigning a new one if not seen before
int LVN::getVN(int reg) {
    if (reg < 0) {
        return -1;
    }

    // find existing VN for this register
    auto it = registerToVN.find(reg);
    if (it != registerToVN.end()) {
        return it->second;
    }

    int vn = newVN();
    registerToVN[reg] = vn; // assign new VN to this register
    return vn;
}

// define that a register has a value number (overwrites any existing mapping)
void LVN::define(int reg, int vn) {
    if (reg < 0) {
        return;
    }

    // finding old VN for this register,
    auto old = registerToVN.find(reg);
    if (old != registerToVN.end()) { // if not found
        int oldVN = old->second; 
        auto cr = vnReg.find(oldVN); // find the register that oldVN maps to and erase it
        if (cr != vnReg.end() && cr->second == reg) {
            vnReg.erase(cr);
        }
    }
    registerToVN[reg] = vn; // assign new VN to this register
}


// get canonical value number for a register (f
int LVN::canonical(int reg) const {
    if (reg < 0) {
        return reg;
    }

    // find register that maps to the same VN as this register
    auto rv = registerToVN.find(reg);
    if (rv == registerToVN.end()) { // if not found, return original register
        return reg;
    }

    auto cr = vnReg.find(rv->second); // find the register that maps to the same VN as this register
    if (cr == vnReg.end()) { // if not found, return original register
        return reg;
    }

    return cr->second; // return the canonical register that maps to the same VN as this register
}

// get a string key for an expression (op, vn1, vn2)
std::string LVN::exprKey(TokenType op, int vn1, int vn2) const {
    if (op == TOKEN_ADD || op == TOKEN_MULT) { // order add and mult operands in ascending order
        if (vn1 > vn2) {
            std::swap(vn1, vn2);
        }
    }
    //return string key
    return std::to_string(static_cast<int>(op)) + "," + std::to_string(vn1) + "," + std::to_string(vn2); 
}

// fold 2 constants for an operation
std::optional<long long> LVN::fold(TokenType op, long long c1, long long c2) const {
    switch (op) { // only fold if both operands are constants

        case TOKEN_ADD:    
            return c1 + c2;
        case TOKEN_SUB:   
            return c1 - c2;
        case TOKEN_MULT:   
            return c1 * c2;

        case TOKEN_LSHIFT:  //shifts need casting to avoid negative shift counts and sign-extension issues
            return static_cast<long long>(static_cast<unsigned long long>(c1) << c2);
        case TOKEN_RSHIFT: 
            return static_cast<long long>(static_cast<unsigned long long>(c1) >> c2);

        //if not a foldable operation, return nullopt
        default:           
            return std::nullopt;
    }
}

// remove a node from the IR linked list and return new head
IRNode* LVN::removeNode(IRNode* node, IRNode* head) {
    if (node->prev) { // fix prev's next pointer
        node->prev->next = node->next;
    }

    if (node->next) { // fix next's prev pointer
        node->next->prev = node->prev;
    }

    IRNode* newHead;
    if (node == head) { // if removing head, update head pointer
        newHead = node->next;
    } else {
        newHead = head; 
    }

    delete node;
    return newHead;
}


// main LVN forward pass
IRNode* LVN::lvnPass(IRNode* head) {

    IRNode* node = head;
    while (node) { //iterate through IR nodes
        IRNode* next = node->next;

        switch (node->opcode) { // handle each opcode type

            // load immediate
            case TOKEN_LOADI: {
                long long constVal = static_cast<long long>(node->sr1); // get constant value from sr1
                std::string key = "CONST," + std::to_string(constVal); // create key for this constant

                int vn; 
                auto eit = exprToVN.find(key); // check if this constant already has a VN
                if (eit != exprToVN.end()) { // if found, reuse existing VN
                    vn = eit->second;
                } else { // if not found, create new VN and record it
                    vn = newVN();
                    exprToVN[key] = vn;
                    vnToConst[vn] = constVal;
                }

                // define that sr3 has this VN, and record that this VN maps to sr3
                define(node->sr3, vn);
                if (vnReg.find(vn) == vnReg.end()) { // set vnReg mapping if not already set
                    vnReg[vn] = node->sr3;
                }

                break;
            }

            // arithmetic and shift operations
            case TOKEN_ADD:
            case TOKEN_SUB:
            case TOKEN_MULT:
            case TOKEN_LSHIFT:
            case TOKEN_RSHIFT: {
                //get canonical registers for operands
                node->sr1 = canonical(node->sr1);
                node->sr2 = canonical(node->sr2);

                //get VNs for operands
                int vn1 = getVN(node->sr1);
                int vn2 = getVN(node->sr2);

                // check if both operands are constants
                auto c1it = vnToConst.find(vn1);
                auto c2it = vnToConst.find(vn2);

                if (c1it != vnToConst.end() && c2it != vnToConst.end()) { // if both operands are constants, try to fold
                    auto result = fold(node->opcode, c1it->second, c2it->second);

                    if (result.has_value()) { // if fold succeeded, 

                        long long folded = result.value(); // get folded constant value

                        // transform this node into a loadI of the folded constant
                        node->opcode = TOKEN_LOADI;
                        node->sr1 = static_cast<int>(folded);
                        node->sr2 = -1;

                        // check if this folded constant already has a VN
                        std::string ckey = "CONST," + std::to_string(folded);

                        int vn;
                        auto ceit = exprToVN.find(ckey);
                        if (ceit != exprToVN.end()) { // if found, reuse existing VN
                            vn = ceit->second;
                        } else { // if not found, create new VN and record it
                            vn = newVN();
                            exprToVN[ckey] = vn;
                            vnToConst[vn]  = folded;
                        }

                        define(node->sr3, vn);
                        if (vnReg.find(vn) == vnReg.end()) {
                            vnReg[vn] = node->sr3;
                        }
                        break;
                    }
                }

                // if not foldable
                std::string key = exprKey(node->opcode, vn1, vn2); // create key for this expression

                auto eit = exprToVN.find(key);
                if (eit != exprToVN.end()) { // if found, reuse existing VN 

                    int existingVN  = eit->second;
                    auto rit = vnReg.find(existingVN);

                    if (rit != vnReg.end()) { // if the existing VN maps to a register, reuse that register

                        define(node->sr3, existingVN);
                        // Remove this node as its value is already computed.
                        head = removeNode(node, head);
                        node = next;
                        continue;
                    }
                }

                // create new VN for this expression and record it
                int vn = newVN();
                exprToVN[key] = vn;

                define(node->sr3, vn);
                vnReg[vn] = node->sr3;
                break;
            }

            // load operation
            case TOKEN_LOAD: {
                // get canonical register for source
                node->sr1 = canonical(node->sr1);
                int vn = newVN();
                define(node->sr3, vn); //use fresh VN and define it for sr3
                vnReg[vn] = node->sr3;
                break;
            }

            // store operation
            case TOKEN_STORE: {
                // get canonical registers for source and destination
                node->sr1 = canonical(node->sr1);
                node->sr3 = canonical(node->sr3);
                break;
            }

            // do nothing for output and nop
            case TOKEN_OUTPUT:
            case TOKEN_NOP:
            default:
                break;
        }

        node = next; // move to next node 
    }

    return head;
}

// backward dead code elimination pass
IRNode* LVN::deadCodeElimination(IRNode* head) {
    bool changed = true;
    while (changed) {
        changed = false; // change flag to track if we removed any nodes in this iteration
 
        // Walk to tail
        IRNode* n = head;
        while (n && n->next) n = n->next;
 
        std::unordered_set<int> live; // set of live registers
 
        while (n) {
            IRNode* prev = n->prev;
 
            bool hasSideEffect = (n->opcode == TOKEN_STORE || n->opcode == TOKEN_OUTPUT);
            int dest = -1;

            // determine destination register for this instruction
            switch (n->opcode) {
                case TOKEN_LOADI:
                case TOKEN_LOAD:
                case TOKEN_ADD: 
                case TOKEN_SUB: 
                case TOKEN_MULT:
                case TOKEN_LSHIFT: 
                case TOKEN_RSHIFT:
                    dest = n->sr3; 
                    break;
                default: break;
            }
 
            // If instruction has no side effects and its destination is not live, it is dead and can be removed.
            if (!hasSideEffect && dest >= 0 && live.find(dest) == live.end()) {
                head = removeNode(n, head);
                changed = true;
                n = prev;
                continue;
            }
 
            // Instruction is kept: def kills liveness, uses add liveness.
            if (dest >= 0) {
                live.erase(dest);
            }
 
            // Add source registers to live set
            switch (n->opcode) {
                case TOKEN_LOAD:
                    if (n->sr1 >= 0) {
                        live.insert(n->sr1);
                    }
                    break;

                case TOKEN_STORE:
                    if (n->sr1 >= 0) {
                        live.insert(n->sr1);
                    }
                    if (n->sr3 >= 0) {
                        live.insert(n->sr3);
                    }
                    break;

                case TOKEN_ADD: 
                case TOKEN_SUB: 
                case TOKEN_MULT:
                case TOKEN_LSHIFT: 
                case TOKEN_RSHIFT:
                    if (n->sr1 >= 0) {
                        live.insert(n->sr1);
                    }
                    if (n->sr2 >= 0) {
                        live.insert(n->sr2);
                    }
                    break;

                default: 
                    break;
            }
 
            n = prev; // move to previous node
        }
    }
    return head; // return new head after DCE
}


// entry point for LVN optimization
IRNode* LVN::optimize(IRNode* head) {
    if (!head) { // if empty IR, just return it
        return head;
    }

    head = lvnPass(head); // first do LVN pass to optimize and annotate with VNs
    head = deadCodeElimination(head); // then do DCE pass to remove any dead code exposed by LVN optimizations

    return head;
}


// print helpers
void LVN::printNode(IRNode* node) const {
    switch (node->opcode) {
        case TOKEN_LOADI:
            std::cout << "loadI " << node->sr1 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_LOAD:
            std::cout << "load r" << node->sr1 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_STORE:
            std::cout << "store r" << node->sr1 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_ADD:
            std::cout << "add r" << node->sr1 << ", r" << node->sr2 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_SUB:
            std::cout << "sub r" << node->sr1 << ", r" << node->sr2 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_MULT:
            std::cout << "mult r" << node->sr1 << ", r" << node->sr2 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_LSHIFT:
            std::cout << "lshift r" << node->sr1 << ", r" << node->sr2 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_RSHIFT:
            std::cout << "rshift r" << node->sr1 << ", r" << node->sr2 << " => r" << node->sr3 << "\n";
            break;
        case TOKEN_OUTPUT:
            std::cout << "output " << node->sr1 << "\n";
            break;

        case TOKEN_NOP:
            break;
        default:
            break;
    }
}

// print the entire IR linked list
void LVN::printIR(IRNode* head) const {
    for (IRNode* n = head; n; n = n->next)
        printNode(n);
}