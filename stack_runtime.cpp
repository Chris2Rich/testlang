// stack_runtime.cpp - Runtime library for stack operations
#include <iostream>
#include <stack>
#include <vector>
#include <cstdlib>
#include <cstring>

// Stack for doubles
static std::stack<double> doubleStack;

// Array structure matching LLVM IR
struct Array {
    int size;
    double* data;
};

// Stack for arrays
static std::stack<Array> arrayStack;

extern "C" {
    // Double stack operations
    void push_double(double val) {
        doubleStack.push(val);
    }
    
    double pop_double() {
        if (doubleStack.empty()) {
            std::cerr << "Runtime Error: Attempted to pop from empty stack" << std::endl;
            return 0.0;
        }
        double val = doubleStack.top();
        doubleStack.pop();
        return val;
    }
    
    // Array stack operations
    void push_array(Array arr) {
        // Make a deep copy of the array
        Array copy;
        copy.size = arr.size;
        copy.data = (double*)malloc(sizeof(double) * arr.size);
        memcpy(copy.data, arr.data, sizeof(double) * arr.size);
        arrayStack.push(copy);
    }
    
    Array pop_array() {
        if (arrayStack.empty()) {
            std::cerr << "Runtime Error: Attempted to pop array from empty stack" << std::endl;
            Array empty = {0, nullptr};
            return empty;
        }
        Array arr = arrayStack.top();
        arrayStack.pop();
        return arr;
    }
    
    // Print functions
    void print_double(double val) {
        std::cout << val << std::endl;
    }
    
    void print_array(Array arr) {
        std::cout << "[";
        for (int i = 0; i < arr.size; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << arr.data[i];
        }
        std::cout << "]" << std::endl;
    }
    
    // Memory management
    extern "C" void* runtime_malloc(size_t size) {
        return std::malloc(size);
    }
    
    void free(void* ptr) {
        std::free(ptr);
    }
    
    // Additional utility functions
    void stack_debug_double() {
        std::cout << "Double stack size: " << doubleStack.size() << std::endl;
    }
    
    void stack_debug_array() {
        std::cout << "Array stack size: " << arrayStack.size() << std::endl;
    }
    
    // Cleanup function (called at program exit)
    void cleanup_stacks() {
        // Clean up any remaining arrays
        while (!arrayStack.empty()) {
            Array arr = arrayStack.top();
            arrayStack.pop();
            if (arr.data) {
                free(arr.data);
            }
        }
    }
}

// Register cleanup function
static void __attribute__((constructor)) init_runtime() {
    std::atexit(cleanup_stacks);
}