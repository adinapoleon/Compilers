#include "scanner.h"
#include <cctype>
#include <unordered_map>

//map to identify opcode
std::unordered_map<std::string, TokenType> opcodeMap = {
    {"load", TOKEN_LOAD},
    {"loadI", TOKEN_LOADI},
    {"store", TOKEN_STORE},
    {"add", TOKEN_ADD},
    {"sub", TOKEN_SUB},
    {"mult", TOKEN_MULT},
    {"lshift", TOKEN_LSHIFT},
    {"rshift", TOKEN_RSHIFT},
    {"output", TOKEN_OUTPUT},
    {"nop", TOKEN_NOP}
};

//constructor
Scanner::Scanner(const std::string& filename) 
    : curr_size(0), pos(0), line(1) {
    
    input.open(filename);
    if (!input.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(1);
    }
    
    // fill  buffer
    fillBuffer();
}


bool Scanner::fillBuffer() {
    //check if input is good
    if (!input.good()) {
        curr_size = 0;
        return false;
    }

    input.read(buffer, BUFSIZE);
    curr_size = input.gcount();
    pos = 0;
    return curr_size > 0;  //return if buffer is filled 
}

char Scanner::peek() {
    if (pos >= curr_size) {
        //load next chunk into buffer
        if (!fillBuffer()) {
            return '\0'; //if fill buffer fails then end of file
        }
    }

    return buffer[pos];
}

char Scanner::get() {
    //peek to see next char
    char c = peek();
    
    if (c == '\0') { //if end of line, no need to move pos
        return '\0';
    }

    pos++; //move pos
    
    if (c == '\n') { //add line if newline
        line++;
    }

    return c;  //return c
}

void Scanner::skipWhitespace() {
    //get until newline
    while (std::isspace(peek()) && peek() != '\n') {
        get();
    }
}

//main scanner function
Token Scanner::nextToken() {
    skipWhitespace(); //skip all whitespace

    char c = peek();

    if (c == '\0') {
        return {TOKEN_EOF, line, ""};
    }

    if (c == '/') {
        get();

        //skip comment
        if (peek() == '/') {
            while (peek() != '\n' && peek() != '\0') {
                get();
            }
            return nextToken();
        }

        //if not 2 //, then error
        return {TOKEN_ERROR, line, "/"};
    }

    switch (c) {
        case '\n': { //newline
            get();
            return {TOKEN_EOL, line-1, "\\n"};
        }

        case ',': {  //comma
            get();
            return {TOKEN_COMMA, line, ","};
        }

        case '=': {
            get();
            if (peek() == '>') { //check if correct arrow syntax
                get();
                return {TOKEN_ARROW, line, "=>"};
            }
            return {TOKEN_ERROR, line, "="};
        }

        default:
            break;
    }

    //regirsters and opcodes
    if (std::isalpha(c)) {
        std::string lex;
        lex += get(); // include the first character

        while (std::isalnum(peek())) {
            lex += get();
        }

        // check for register
        if (lex[0] == 'r') {
            bool allDigits = true;
            for (size_t i = 1; i < lex.size(); i++) {
                if (!isdigit(lex[i])) {
                    allDigits = false;
                }
            }

            if (allDigits && lex.size() > 1) {
                return {TOKEN_REGISTER, line, lex};
            }
        }

        // check opcode map
        if (opcodeMap.count(lex)) {
            return {opcodeMap[lex], line, lex};
        }

        return {TOKEN_ERROR, line, lex};
    }


    //Constant
    if (std::isdigit(c)) {
        std::string lex;

        while (std::isdigit(peek())) {
            lex += get();
        }

        return {TOKEN_CONSTANT, line, lex};
    }

    //if we get here, then there is junk
    std::string bad(1, get());
    return {TOKEN_ERROR, line, bad};
}

//helper for -s flag
std::string Scanner::tokenTypetoString(TokenType t) {
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

void Scanner::scanAll() {
    while (true) {
        Token t = nextToken();
        if (t.type == TOKEN_EOF) {
            break;
        }

        std::cout << t.line << " "
                << tokenTypetoString(t.type) << " " 
                << t.lexeme << std::endl;
    }
}