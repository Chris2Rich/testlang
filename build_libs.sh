#!/bin/bash

# build_libs.sh - Build script for Testlang libraries

set -e

echo "Building Testlang liraries..."

# Build the libraries
for file in ./libraries/*.cpp; do
    filename="${file%.*}"
    echo "Compiling $filename..."
    clang++ -std=c++17 -O3 -fPIC -c $file -o $filename.o
    ar rcs libstack_runtime.a stack_runtime.o
done

echo "Compiled all libraries!"