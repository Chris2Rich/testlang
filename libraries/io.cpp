#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <stack>
#include "../stack_runtime.h"

extern "C" {

// io.input_str - Read a line from stdin as UTF-8 string, convert to array
// Each double holds 2 UTF-8 characters (8 bytes)
void input_str() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        // EOF or error - push empty array
        std::vector<long> shape = {0};
        valueStack.push(new Value(shape, nullptr));
        return;
    }

    // Remove trailing newline if present
    if (!line.empty() && line.back() == '\n') {
        line.pop_back();
    }

    // Calculate how many doubles we need (2 chars per double = 8 bytes)
    size_t num_doubles = (line.length() + 1) / 2 + 1;  // +1 for safety
    
    double *data = (double *)malloc(sizeof(double) * num_doubles);
    memset(data, 0, sizeof(double) * num_doubles);
    
    // Copy string bytes into double array (2 chars per double)
    memcpy(data, line.c_str(), line.length());
    
    std::vector<long> shape = {static_cast<long>(line.length())};
    valueStack.push(new Value(shape, data));
    free(data);
}

// Helper function to parse an array from string like "[1, 2, 3]"
static bool parse_array(const std::string &str, std::vector<double> &out_data) {
    size_t start = str.find('[');
    size_t end = str.find(']');
    
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return false;
    }
    
    std::string content = str.substr(start + 1, end - start - 1);
    std::stringstream ss(content);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        size_t first = token.find_first_not_of(" \t");
        size_t last = token.find_last_not_of(" \t");
        if (first == std::string::npos) continue;
        std::string trimmed = token.substr(first, last - first + 1);
        
        if (!trimmed.empty()) {
            try {
                out_data.push_back(std::stod(trimmed));
            } catch (...) {
                return false;
            }
        }
    }
    
    return !out_data.empty();
}

// io.input - Read a line from stdin and parse as number or array
void input() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        // EOF - push 0
        valueStack.push(new Value(0.0));
        return;
    }

    // Trim whitespace
    size_t first = line.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        valueStack.push(new Value(0.0));
        return;
    }
    size_t last = line.find_last_not_of(" \t\n\r");
    std::string trimmed = line.substr(first, last - first + 1);

    // Check if it's an array (starts with [)
    if (trimmed[0] == '[') {
        std::vector<double> data;
        if (parse_array(trimmed, data)) {
            std::vector<long> shape = {static_cast<long>(data.size())};
            valueStack.push(new Value(shape, data.data()));
        } else {
            // Failed to parse as array - push empty array
            std::vector<long> shape = {0};
            valueStack.push(new Value(shape, nullptr));
        }
        return;
    }

    // Try to parse as number
    try {
        size_t pos;
        double num = std::stod(trimmed, &pos);
        if (pos == trimmed.length()) {
            valueStack.push(new Value(num));
        } else {
            // Not a complete number - treat as string (input_str behavior)
            size_t num_doubles = (trimmed.length() + 1) / 2 + 1;
            double *data = (double *)malloc(sizeof(double) * num_doubles);
            memset(data, 0, sizeof(double) * num_doubles);
            memcpy(data, trimmed.c_str(), trimmed.length());
            std::vector<long> shape = {static_cast<long>(trimmed.length())};
            valueStack.push(new Value(shape, data));
            free(data);
        }
    } catch (...) {
        // Not a number - treat as string (input_str behavior)
        size_t num_doubles = (trimmed.length() + 1) / 2 + 1;
        double *data = (double *)malloc(sizeof(double) * num_doubles);
        memset(data, 0, sizeof(double) * num_doubles);
        memcpy(data, trimmed.c_str(), trimmed.length());
        std::vector<long> shape = {static_cast<long>(trimmed.length())};
        valueStack.push(new Value(shape, data));
        free(data);
    }
}

} // extern "C"
