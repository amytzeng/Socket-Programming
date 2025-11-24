# Makefile for P2P Micropayment System - Client Program
# Course: Computer Networks (Fall 2025)
# Professor: Yeali S. Sun
# Phase 1: Client Implementation with P2P Money Transfer
#
# This Makefile compiles the client program for Phase 1 submission.
# Usage: make        - Compile the client program
#        make clean  - Remove all compiled files
#        make rebuild - Clean and recompile

# Compiler
CXX = g++

# Compiler flags
# -std=c++11  : Use C++11 standard features
# -Wall       : Enable all common warnings
# -Wextra     : Enable extra warnings
# -pthread    : Link pthread library for multi-threading support
# -O2         : Optimization level 2 for better performance
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -O2

# Uncomment the following line for debugging (adds debug symbols and disables optimization)
# CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -g -O0 -DDEBUG

# Target executable (required for submission)
TARGET = client

# Source files
SOURCES = client.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default target - builds the client program
# This is what TAs will run with 'make' command
all: $(TARGET)
	@echo "============================================"
	@echo "  Client program compiled successfully!"
	@echo "  Executable: ./$(TARGET)"
	@echo "============================================"

# Build the client executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJECTS)
	@echo "============================================"
	@echo "  Clean complete - all build files removed"
	@echo "============================================"

# Rebuild everything from scratch
rebuild: clean all

# Run the client program
run: $(TARGET)
	./$(TARGET)

# Help target - display available commands
help:
	@echo "============================================"
	@echo "  P2P Micropayment Client - Makefile Help"
	@echo "============================================"
	@echo "Available targets:"
	@echo "  make         - Build the client program (default)"
	@echo "  make clean   - Remove all compiled files"
	@echo "  make rebuild - Clean and rebuild from scratch"
	@echo "  make run     - Build and run the client"
	@echo "  make help    - Show this help message"
	@echo "============================================"

# Declare phony targets (targets that don't represent actual files)
.PHONY: all clean rebuild run help

