#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <cstddef>

//all token categories
enum TokenType {
    //ILOC instructions
    TOKEN_LOAD,
    TOKEN_LOADI,
    TOKEN_STORE,
    TOKEN_ADD,
    TOKEN_SUB,
    TOKEN_MULT,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,
    TOKEN_OUTPUT,
    TOKEN_NOP,

    TOKEN_REGISTER,   //r followed by digits
    TOKEN_CONSTANT,   // non negative integer
    TOKEN_COMMA,   // ,
    TOKEN_ARROW,   // =>
    TOKEN_EOL,  // end of line
    TOKEN_EOF,  // end of file
    TOKEN_ERROR  // error
};


//token struct
struct Token {
    TokenType type;
    int line;    // source line number
    std::string lexeme;  // spelling of opcode, register, 
};


//scanner class
class Scanner {
private:
    static constexpr size_t BUFSIZE = 16 * 1024; //buffer size (using 16kb buffer b/c 120,000 lines)

    std::ifstream input;   // file input stream
    char buffer[BUFSIZE];   //input buffer

    size_t curr_size;   //num char is current buffer
    size_t pos;     //curr index in buffer
    int line;       //current line 

    bool fillBuffer();   //load next block from file
    char peek();     //look at next char without advancing
    char get();        // get next char
    
    //skip functions    
    void skipWhitespace();

    //helper
    std::string tokenTypetoString(TokenType T);

public:
    //explicit constructor to prevent type conversions
    explicit Scanner(const std::string& filename);

    Token nextToken();
    void scanAll();  //for -s flag
};

