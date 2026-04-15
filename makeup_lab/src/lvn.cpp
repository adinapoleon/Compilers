#include "lvn.h"
#include <iostream>
#include <algorithm>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
LVN::LVN() : nextVN(0) {}

// ---------------------------------------------------------------------------
// Value number helpers
// ---------------------------------------------------------------------------

int LVN::newVN() { return nextVN++; }

int LVN::getVN(int reg) {
    if (reg < 0) return -1;
    auto it = regVN.find(reg);
    if (it != regVN.end()) return it->second;
    int vn = newVN();
    regVN[reg] = vn;
    return vn;
}

void LVN::define(int reg, int vn) {
    if (reg < 0) return;
    auto old = regVN.find(reg);
    if (old != regVN.end()) {
        int oldVN = old->second;
        auto cr = vnReg.find(oldVN);
        if (cr != vnReg.end() && cr->second == reg)
            vnReg.erase(cr);
    }
    regVN[reg] = vn;
}

int LVN::canonical(int reg) const {
    if (reg < 0) return reg;
    auto rv = regVN.find(reg);
    if (rv == regVN.end()) return reg;
    auto cr = vnReg.find(rv->second);
    if (cr == vnReg.end()) return reg;
    return cr->second;
}

std::string LVN::exprKey(TokenType op, int vn1, int vn2) const {
    if (op == TOKEN_ADD || op == TOKEN_MULT) {
        if (vn1 > vn2) std::swap(vn1, vn2);
    }
    return std::to_string(static_cast<int>(op))
         + "," + std::to_string(vn1)
         + "," + std::to_string(vn2);
}

std::optional<long long> LVN::fold(TokenType op, long long c1, long long c2) const {
    switch (op) {
        case TOKEN_ADD:    return c1 + c2;
        case TOKEN_SUB:    return c1 - c2;
        case TOKEN_MULT:   return c1 * c2;
        case TOKEN_LSHIFT: return static_cast<long long>(
                               static_cast<unsigned long long>(c1) << c2);
        case TOKEN_RSHIFT: return static_cast<long long>(
                               static_cast<unsigned long long>(c1) >> c2);
        default:           return std::nullopt;
    }
}

IRNode* LVN::removeNode(IRNode* node, IRNode* head) {
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    IRNode* newHead = (node == head) ? node->next : head;
    delete node;
    return newHead;
}

// ---------------------------------------------------------------------------
// Forward LVN pass
// ---------------------------------------------------------------------------

IRNode* LVN::lvnPass(IRNode* head) {
    IRNode* node = head;
    while (node) {
        IRNode* next = node->next;

        switch (node->opcode) {

            // ---- loadI const => r3 ----------------------------------------
            case TOKEN_LOADI: {
                long long constVal = static_cast<long long>(node->sr1);
                std::string key = "CONST," + std::to_string(constVal);
                int vn;
                auto eit = exprVN.find(key);
                if (eit != exprVN.end()) {
                    vn = eit->second;
                } else {
                    vn = newVN();
                    exprVN[key] = vn;
                    vnConst[vn] = constVal;
                }
                define(node->sr3, vn);
                if (vnReg.find(vn) == vnReg.end())
                    vnReg[vn] = node->sr3;
                break;
            }

            // ---- Arithmetic: op r1, r2 => r3 --------------------------------
            case TOKEN_ADD:
            case TOKEN_SUB:
            case TOKEN_MULT:
            case TOKEN_LSHIFT:
            case TOKEN_RSHIFT: {
                // Copy propagation on sources
                node->sr1 = canonical(node->sr1);
                node->sr2 = canonical(node->sr2);

                int vn1 = getVN(node->sr1);
                int vn2 = getVN(node->sr2);

                // Constant folding
                auto c1it = vnConst.find(vn1);
                auto c2it = vnConst.find(vn2);
                if (c1it != vnConst.end() && c2it != vnConst.end()) {
                    auto result = fold(node->opcode, c1it->second, c2it->second);
                    if (result.has_value()) {
                        long long folded = result.value();
                        node->opcode = TOKEN_LOADI;
                        node->sr1    = static_cast<int>(folded);
                        node->sr2    = -1;

                        std::string ckey = "CONST," + std::to_string(folded);
                        int vn;
                        auto ceit = exprVN.find(ckey);
                        if (ceit != exprVN.end()) {
                            vn = ceit->second;
                        } else {
                            vn = newVN();
                            exprVN[ckey] = vn;
                            vnConst[vn]  = folded;
                        }
                        define(node->sr3, vn);
                        if (vnReg.find(vn) == vnReg.end())
                            vnReg[vn] = node->sr3;
                        break;
                    }
                }

                // CSE
                std::string key = exprKey(node->opcode, vn1, vn2);
                auto eit = exprVN.find(key);
                if (eit != exprVN.end()) {
                    int existingVN  = eit->second;
                    auto rit = vnReg.find(existingVN);
                    if (rit != vnReg.end()) {
                        // Alias sr3 -> existingVN so downstream uses resolve correctly.
                        define(node->sr3, existingVN);
                        // Remove this node — its value is already computed.
                        head = removeNode(node, head);
                        node = next;
                        continue;
                    }
                }

                // New expression
                int vn = newVN();
                exprVN[key] = vn;
                define(node->sr3, vn);
                vnReg[vn] = node->sr3;
                break;
            }

            // ---- load r1 => r3 ----------------------------------------------
            case TOKEN_LOAD: {
                node->sr1 = canonical(node->sr1);
                int vn = newVN();       // fresh VN — memory content unknown
                define(node->sr3, vn);
                vnReg[vn] = node->sr3;
                break;
            }

            // ---- store r1 => r3 ---------------------------------------------
            case TOKEN_STORE: {
                node->sr1 = canonical(node->sr1);
                node->sr3 = canonical(node->sr3);
                break;
            }

            // ---- output / nop -----------------------------------------------
            case TOKEN_OUTPUT:
            case TOKEN_NOP:
            default:
                break;
        }

        node = next;
    }
    return head;
}

// ---------------------------------------------------------------------------
// Backward DCE pass
// Iterates until no more dead instructions are found (handles chains like
// "loadI => r1; add r1,r2 => r3" where r3 is dead — first iter kills r3,
// second iter may kill r1 if nothing else uses it).
// ---------------------------------------------------------------------------

IRNode* LVN::dce(IRNode* head) {
    bool changed = true;
    while (changed) {
        changed = false;

        // Forward pass: collect all registers that are READ somewhere.
        std::unordered_set<int> live;
        for (IRNode* n = head; n; n = n->next) {
            switch (n->opcode) {
                case TOKEN_LOAD:
                    if (n->sr1 >= 0) live.insert(n->sr1);
                    break;
                case TOKEN_STORE:
                    if (n->sr1 >= 0) live.insert(n->sr1);
                    if (n->sr3 >= 0) live.insert(n->sr3);
                    break;
                case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT:
                case TOKEN_LSHIFT: case TOKEN_RSHIFT:
                    if (n->sr1 >= 0) live.insert(n->sr1);
                    if (n->sr2 >= 0) live.insert(n->sr2);
                    break;
                // loadI, output, nop: no register reads that contribute to liveness
                default:
                    break;
            }
        }

        // Backward pass: remove instructions whose dest is not live and have
        // no side effects.
        IRNode* n = head;
        // Walk to tail first for proper backward order
        while (n && n->next) n = n->next;

        while (n) {
            IRNode* prev = n->prev;

            bool hasSideEffect = (n->opcode == TOKEN_STORE ||
                                  n->opcode == TOKEN_OUTPUT);
            int dest = -1;
            switch (n->opcode) {
                case TOKEN_LOADI:
                case TOKEN_LOAD:
                    dest = n->sr3; break;
                case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT:
                case TOKEN_LSHIFT: case TOKEN_RSHIFT:
                    dest = n->sr3; break;
                default:
                    break;
            }

            if (!hasSideEffect && dest >= 0 && live.find(dest) == live.end()) {
                // Dead instruction — remove it
                head    = removeNode(n, head);
                changed = true;
            }

            n = prev;
        }
    }
    return head;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

IRNode* LVN::optimize(IRNode* head) {
    if (!head) return head;
    head = lvnPass(head);
    head = dce(head);
    return head;
}

// ---------------------------------------------------------------------------
// Print
// ---------------------------------------------------------------------------

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
            // Drop nops — they waste cycles
            break;
        default:
            break;
    }
}

void LVN::printIR(IRNode* head) const {
    for (IRNode* n = head; n; n = n->next)
        printNode(n);
}