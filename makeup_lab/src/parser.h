#pragma once
#include "scanner.h"

struct IRNode {
    //  Intermediate Representation Node structure
    int line;
    TokenType opcode;

    // feilds for IR
    int sr1 = -1, vr1 = -1, pr1 = -1, nu1 = -1;
    int sr2 = -1, vr2 = -1, pr2 = -1, nu2 = -1;
    int sr3 = -1, vr3 = -1, pr3 = -1, nu3 = -1;

    IRNode* prev = nullptr;
    IRNode* next = nullptr;
};

class Parser {
public:
    Parser(Scanner& scanner); //constructor
    
    IRNode* parseAll(); // return head of IR linked list
    void printIR(); //print the IR linked list

private:
    Scanner& scanner;
    Token lookahead;

    IRNode* head = nullptr; // head of IR linked list
    IRNode* tail = nullptr; // tail of IR linked list

    //helper functions
    bool match(TokenType expected);
    bool expect(TokenType expected, const std::string& errorMessage);
    void addIRNode(IRNode* Node);
    int getRegisterNumber(const std::string& lexeme);
    int getConstantValue(const std::string& lexeme);
    void skiptoEOL();

    //parsing functions
    bool parseOperation();
    bool parseLoad(IRNode* node);
    bool parseLoadI(IRNode* node);
    bool parseStore(IRNode* node);
    bool parseArithmetic(IRNode* node);
    bool parseOutput(IRNode* node);
    bool parseNop(IRNode* );

    // IR printing helper
    std::string tokenTypeToString(TokenType T);
    void printIRNode(IRNode* node);
};