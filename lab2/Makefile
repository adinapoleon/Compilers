# CSCE 434 Lab1 Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = 434alloc

SRC = src/main.cpp src/scanner.cpp src/cli2.cpp src/parser.cpp src/renamer.cpp
OBJ = $(SRC:.cpp=.o)

.PHONY: clean build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

