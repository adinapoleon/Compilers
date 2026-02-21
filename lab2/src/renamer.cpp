#include "renamer.h"
#include <iostream>

RegisterRenamer::RegisterRenamer() : nextNewReg(0) {}

void RegisterRenamer::reset() {
    regMap.clear();
    nextNewReg = 0;
}

int RegisterRenamer::getNewRegister(int oldReg) {
    if (oldReg == -1) return -1;

    auto it = regMap.find(oldReg);
    if (it != regMap.end())
        return it->second;

    int newReg = nextNewReg++;
    regMap[oldReg] = newReg;
    return newReg;
}

void RegisterRenamer::processInstruction(IRNode* node) {
    switch (node->opcode) {

        case TOKEN_LOAD:
            node->vr1 = getNewRegister(node->sr1);
            node->vr3 = getNewRegister(node->sr3);
            break;

        case TOKEN_STORE:
            node->vr1 = getNewRegister(node->sr1);
            node->vr3 = getNewRegister(node->sr3);
            break;

        case TOKEN_LOADI:
            node->vr3 = getNewRegister(node->sr3);
            break;

        case TOKEN_ADD:
        case TOKEN_SUB:
        case TOKEN_MULT:
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
            node->vr1 = getNewRegister(node->sr1);
            node->vr2 = getNewRegister(node->sr2);
            node->vr3 = getNewRegister(node->sr3);
            break;

        default:
            break;
    }
}

void RegisterRenamer::rename(IRNode* head) {
    reset();

    IRNode* current = head;
    while (current != nullptr) {
        processInstruction(current);
        current = current->next;
    }
}

void RegisterRenamer::printInstruction(IRNode* node) {
    switch (node->opcode) {

        case TOKEN_LOAD:
            std::cout << "load r" << node->vr1
                      << " => r" << node->vr3;
            break;

        case TOKEN_LOADI:
            std::cout << "loadI " << node->sr1
                      << " => r" << node->vr3;
            break;

        case TOKEN_STORE:
            std::cout << "store r" << node->vr1
                      << " => r" << node->vr3;
            break;

        case TOKEN_ADD:
            std::cout << "add r" << node->vr1
                      << ", r" << node->vr2
                      << " => r" << node->vr3;
            break;

        case TOKEN_SUB:
            std::cout << "sub r" << node->vr1
                      << ", r" << node->vr2
                      << " => r" << node->vr3;
            break;

        case TOKEN_MULT:
            std::cout << "mult r" << node->vr1
                      << ", r" << node->vr2
                      << " => r" << node->vr3;
            break;

        case TOKEN_LSHIFT:
            std::cout << "lshift r" << node->vr1
                      << ", r" << node->vr2
                      << " => r" << node->vr3;
            break;

        case TOKEN_RSHIFT:
            std::cout << "rshift r" << node->vr1
                      << ", r" << node->vr2
                      << " => r" << node->vr3;
            break;

        case TOKEN_OUTPUT:
            std::cout << "output " << node->sr1;
            break;

        case TOKEN_NOP:
            std::cout << "nop";
            break;

        default:
            break;
    }

    std::cout << std::endl;
}

void RegisterRenamer::printRenamedIR(IRNode* head) {
    IRNode* current = head;
    while (current != nullptr) {
        printInstruction(current);
        current = current->next;
    }
}