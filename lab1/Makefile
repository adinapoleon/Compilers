# CSCE 434 Lab1 Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = 434fe

SRC = src/main.cpp src/scanner.cpp src/cli.cpp src/parser.cpp 
OBJ = $(SRC:.cpp=.o)

.PHONY: clean build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

