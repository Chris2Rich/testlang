#!/bin/bash

# build.sh - Build script for Stack Language Compiler

set -e

echo "Building Stack Language Compiler..."

# Check if LLVM is installed
if ! command -v llvm-config &> /dev/null; then
    echo "Error: llvm-config not found. Please install LLVM development libraries."
    echo "On Ubuntu/Debian: sudo apt install llvm-dev"
    echo "On macOS: brew install llvm"
    exit 1
fi

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Warning: python3 not found. Make sure Python 3 is installed."
fi

# Build the compiler
echo "Compiling stack compiler..."
clang++ -std=c++17 -O3 \
    testlang.cpp \
    $(llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native) \
    -o testlang

# Build the runtime library
echo "Compiling runtime library..."
clang++ -std=c++17 -O3 -fPIC -c stack_runtime.cpp -o stack_runtime.o
ar rcs libstack_runtime.a stack_runtime.o

echo "Build complete!"
echo ""
echo "Usage examples:"
echo "Direct compilation from source:"
echo "  ./testlang source.stack myprogram"
echo "  ./testlang source.stack output --ir"
echo "  ./testlang source.stack output --obj"
echo ""
echo "With custom lexer path:"
echo "  ./testlang source.stack myprogram --exe /path/to/lexer.py"