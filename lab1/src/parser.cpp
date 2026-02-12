#include "parser.h"
#include <iostream>

//constructor
Parser::Parser(Scanner& scanner) : scanner(scanner) {
    //get first token
    lookahead = scanner.nextToken();
}

// helper to match and cosume token
bool Parser::match(TokenType expected) {
    if (lookahead.type == expected) {
        lookahead = scanner.nextToken();
        return true;
    }
    return false;
}

// helper to expect a specific token, else print error
bool Parser::expect(TokenType expected, const std::string& errorMessage) {
    if (lookahead.type == expected) {
        lookahead = scanner.nextToken();
        return true;
    } else {
        std::cerr << "Parse Error (line " << lookahead.line << "): " << errorMessage << std::endl;
        return false;
    }
}

// add IR node to linked list
void Parser::addIRNode(IRNode* node) {
    if (head == nullptr) {
        head = node;
        tail = node;
    } else {
        tail->next = node;
        node->prev = tail;
        tail = node;
    }
}

// get register number from lexeme
int Parser::getRegisterNumber(const std::string& lexeme) {
    //remove leading 'r
    if (lexeme.empty() || lexeme[0] != 'r') {
        return -1; //invalid register
    }

    try {
        return std::stoi(lexeme.substr(1));
    } catch (...) {
        return -1; //invalid register
    }
}

// get constant value from lexeme
int Parser::getConstantValue(const std::string& lexeme) {
    try {
        return std::stoi(lexeme);
    } catch (...) {
        return -1; //invalid constant
    }
}

// skip to end of line
void Parser::skiptoEOL() {
    while (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        lookahead = scanner.nextToken();
    }
    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }
}

// main parse function
IRNode* Parser::parseAll() {
    bool hasError = false;

    while (lookahead.type != TOKEN_EOF) {
        // skip empty lines
        if (lookahead.type == TOKEN_EOL) {
            lookahead = scanner.nextToken();
            continue;
        }

        //parse operation
        if (!parseOperation()) {
            hasError = true;
            skiptoEOL();
        }
    }

    if (hasError) {
        std::cout << "Errors detected" << std::endl;
        return head; //return partial IR
    }

    std::cout << "Parsing completed successfully." << std::endl;
    return head;
}

//parse functions

// single iloc operation
bool Parser::parseOperation() {
    IRNode* node = new IRNode(); //create new IR node
    node->line = lookahead.line;
    node->opcode = lookahead.type;

    bool success = true; //track if parsing succeeded

    switch(lookahead.type) {
        case TOKEN_LOAD: // load operation
            success = parseLoad(node);
            break;

        case TOKEN_LOADI: // loadi operation
            success = parseLoadI(node);
            break;

        case TOKEN_STORE:  // store operation
            success = parseStore(node);
            break;

        case TOKEN_ADD:
        case TOKEN_SUB:
        case TOKEN_MULT:
        case TOKEN_LSHIFT: // arithmetic operations
        case TOKEN_RSHIFT:
            success = parseArithmetic(node);
            break;

        case TOKEN_OUTPUT: // output operation
            success = parseOutput(node);
            break;

        case TOKEN_NOP: // nop operation
            success = parseNop(node);
            break;

        default: // unexpected token
            std::cerr << "Parse Error (line " << lookahead.line << "): Unexpected token " 
                      << lookahead.lexeme << std::endl;
            delete node;
            return false;
    }

    if (success) {
        addIRNode(node); //add node to IR list
    } else {
        delete node;
    }

    return success;
}

// Parse: load r1 => r2
bool Parser::parseLoad(IRNode* node) {
    if (!match(TOKEN_LOAD)) {
        return false; // if current token is not LOAD
    }

    // get source register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected source register after LOAD." << std::endl;
        return false;
    }
    node->sr1 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // parse arrow
    if (!expect(TOKEN_ARROW, "Expected '=>' after source register in LOAD.")) {
        return false;
    }

    // get destination register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected destination register after '=>' in LOAD." << std::endl;
        return false;
    }
    node->sr3 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after LOAD operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}

// Parse: loadi constant => r2
bool Parser::parseLoadI(IRNode* node) {
    if (!match(TOKEN_LOADI)) {
        return false; // if current token is not LOADI
    }

    // get constant value
    if (lookahead.type != TOKEN_CONSTANT) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected constant after LOADI." << std::endl;
        return false;
    }
    node->sr1 = getConstantValue(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // parse arrow
    if (!expect(TOKEN_ARROW, "Expected '=>' after constant in LOADI.")) {
        return false;
    }

    // get destination register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected destination register after '=>' in LOADI." << std::endl;
        return false;
    }
    node->sr3 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after LOADI operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}

// Parse: store r1 => r2
bool Parser::parseStore(IRNode* node) {
    if (!match(TOKEN_STORE)) {
        return false; // if current token is not STORE
    }

    // get source register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected source register after STORE." << std::endl;
        return false;
    }
    node->sr1 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // parse arrow
    if (!expect(TOKEN_ARROW, "Expected '=>' after source register in STORE.")) {
        return false;
    }

    // get destination register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected destination register after '=>' in STORE." << std::endl;
        return false;
    }
    node->sr3 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after STORE operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}

// Parse: add r1, r2 => r3 (similar for sub, mult, lshift, rshift)
bool Parser::parseArithmetic(IRNode* node) {
    lookahead = scanner.nextToken(); //consume opcode

    // get first source register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected first source register in arithmetic operation." << std::endl;
        return false;
    }
    node->sr1 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // parse comma
    if (!expect(TOKEN_COMMA, "Expected ',' after first source register in arithmetic operation.")) {
        return false;
    }

    // get second source register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected second source register in arithmetic operation." << std::endl;
        return false;
    }
    node->sr2 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // parse arrow
    if (!expect(TOKEN_ARROW, "Expected '=>' after second source register in arithmetic operation.")) {
        return false;
    }

    // get destination register
    if (lookahead.type != TOKEN_REGISTER) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected destination register in arithmetic operation." << std::endl;
        return false;
    }
    node->sr3 = getRegisterNumber(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after arithmetic operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}


// Parse: output constant
bool Parser::parseOutput(IRNode* node) {
    if (!match(TOKEN_OUTPUT)) {
        return false; // if current token is not OUTPUT
    }

    // get constant value
    if (lookahead.type != TOKEN_CONSTANT) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected constant after OUTPUT." << std::endl;
        return false;
    }
    node->sr1 = getConstantValue(lookahead.lexeme);
    lookahead = scanner.nextToken();

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after OUTPUT operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}

// Parse: nop
bool Parser::parseNop(IRNode* node) {
    (void) node; //unused parameter

    if (!match(TOKEN_NOP)) {
        return false; // if current token is not NOP
    }

    // expect end of line
    if (lookahead.type != TOKEN_EOL && lookahead.type != TOKEN_EOF) {
        std::cerr << "Parse Error (line " << lookahead.line << "): Expected end of line after NOP operation." << std::endl;
        return false;
    }

    if (lookahead.type == TOKEN_EOL) {
        lookahead = scanner.nextToken(); //consume EOL
    }

    return true;
}

// IR printing functions
void Parser::printIR() {
    if (head == nullptr) {
        std::cout << "IR is empty." << std::endl;
        return;
    }

    IRNode* current = head;
    while (current != nullptr) {
        printIRNode(current);
        current = current->next;
    }
}

// opcode to string
std::string Parser::tokenTypeToString(TokenType t) {
    switch (t) {
        case TOKEN_LOAD: return "LOAD";
        case TOKEN_LOADI: return "LOADI";
        case TOKEN_STORE: return "STORE";
        case TOKEN_ADD: return "ADD";
        case TOKEN_SUB: return "SUB";
        case TOKEN_MULT: return "MULT";
        case TOKEN_LSHIFT: return "LSHIFT";
        case TOKEN_RSHIFT: return "RSHIFT";
        case TOKEN_OUTPUT: return "OUTPUT";
        case TOKEN_NOP: return "NOP";
        case TOKEN_REGISTER: return "REGISTER";
        case TOKEN_CONSTANT: return "CONSTANT";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_EOL: return "EOL";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

void Parser::printIRNode(IRNode* node) {
    std::cout << "Line " << node->line << ": " << tokenTypeToString(node->opcode);

    switch (node->opcode) {
        case TOKEN_LOAD:
        case TOKEN_STORE:
            std::cout << " [ SR!: r" << node->sr1 
                      << " ] => [ SR3: r]" << node->sr3 << " ]";
            break;

        case TOKEN_LOADI:
            std::cout << " [ SR1: " << node->sr1 
                      << " ] => [ SR3: r" << node->sr3 << " ]";
            break;

        case TOKEN_ADD:
        case TOKEN_SUB:
        case TOKEN_MULT:
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
            std::cout << " [ SR1: r" << node->sr1 
                      << " , SR2: r" << node->sr2 
                      << " ] => [ SR3: r" << node->sr3 << " ]";
            break;

        case TOKEN_OUTPUT:
            std::cout << " [ SR1: " << node->sr1 << " ]";
            break;

        case TOKEN_NOP:
            // no operands
            break;

        default:
            break;
    }

    std::cout << std::endl;
}


