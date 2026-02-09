#include "../stack_runtime.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

// Helper struct to hold intermediate parsing results
struct ParsedResult {
  std::vector<long> shape;
  std::vector<double> data;
};

// Helper to skip whitespace
void skip_whitespace(const std::string &str, size_t &pos) {
  while (pos < str.length() && std::isspace(str[pos])) {
    pos++;
  }
}

// Recursive function to parse arrays and numbers
// Returns a ParsedResult containing the shape and flattened data
ParsedResult parse_recursive(const std::string &str, size_t &pos) {
  skip_whitespace(str, pos);

  if (pos >= str.length()) {
    throw std::runtime_error("Unexpected end of input");
  }

  if (str[pos] == '[') {
    // It's an array
    pos++; // consume '['
    skip_whitespace(str, pos);

    // Check for empty array '[]'
    if (pos < str.length() && str[pos] == ']') {
      pos++; // consume ']'
      return ParsedResult{{0}, {}};
    }

    std::vector<ParsedResult> children;

    while (true) {
      // Parse element
      children.push_back(parse_recursive(str, pos));

      if (pos >= str.length())
        throw std::runtime_error("Unexpected EOF inside array");

      if (str[pos] == ']') {
        pos++; // consume ']'
        break;
      } else if (str[pos] == ',' || std::isspace(str[pos])) {
        pos++; // consume ','
               // continue loop
      } else {
        throw std::runtime_error("Expected ',' or ' ' or ']'");
      }
    }

    if (children.empty())
      return ParsedResult{{0}, {}};

    // Validate consistency (no ragged arrays)
    // All children must have the exact same shape
    const std::vector<long> &first_shape = children[0].shape;
    for (size_t i = 1; i < children.size(); ++i) {
      if (children[i].shape != first_shape) {
        throw std::runtime_error("Ragged arrays are not supported");
      }
    }

    // Construct new shape: [number_of_children, ...child_shape]
    std::vector<long> new_shape;
    new_shape.push_back(children.size());
    new_shape.insert(new_shape.end(), first_shape.begin(), first_shape.end());

    // Flatten data
    std::vector<double> flat_data;
    for (const auto &child : children) {
      flat_data.insert(flat_data.end(), child.data.begin(), child.data.end());
    }

    return ParsedResult{new_shape, flat_data};
  } else {
    // It's a number (base case)
    size_t end_pos;
    try {
      // std::stod parses the double and sets end_pos to the index of the first
      // unconverted char
      std::string substr = str.substr(pos);
      double val = std::stod(substr, &end_pos);
      pos += end_pos;

      // A scalar number technically has an empty shape vector in this recursive
      // logic, but the data contains the value.
      return ParsedResult{{}, {val}};
    } catch (...) {
      throw std::runtime_error("Invalid number format");
    }
  }
}

extern "C" {

// io.input_str - Read a line from stdin as UTF-8 string, convert to array
// (Kept as is per your snippet, assuming this part was working for you)
void input_str() {
  std::string line;
  if (!std::getline(std::cin, line)) {
    std::vector<long> shape = {0};
    valueStack.push(new Value(shape, nullptr));
    return;
  }
  if (!line.empty() && line.back() == '\n')
    line.pop_back();

  size_t num_doubles = (line.length() + 1) / 2 + 1;
  double *data = (double *)malloc(sizeof(double) * num_doubles);
  memset(data, 0, sizeof(double) * num_doubles);
  memcpy(data, line.c_str(), line.length());

  std::vector<long> shape = {static_cast<long>(line.length())};
  valueStack.push(new Value(shape, data));
  free(data);
}

// io.input - Read a line from stdin and parse as number or array
void input() {
  std::string line;
  if (!std::getline(std::cin, line)) {
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

  // 1. Try to parse as Array
  if (trimmed[0] == '[') {
    try {
      size_t pos = 0;
      ParsedResult result = parse_recursive(trimmed, pos);

      // Ensure we consumed the whole string (ignoring trailing whitespace)
      skip_whitespace(trimmed, pos);
      if (pos != trimmed.length()) {
        // If there's garbage after the array, treat as string fallback
        throw std::runtime_error("Garbage after array");
      }

      // Allocate and copy data for the Value object
      // The Value constructor typically takes ownership or copies,
      // relying on malloc here based on your previous code style.
      double *raw_data = nullptr;
      if (!result.data.empty()) {
        raw_data = (double *)malloc(result.data.size() * sizeof(double));
        if (!raw_data) {
          std::cerr << "Memory allocation failed in io.input" << std::endl;
          exit(1);
        }
        memcpy(raw_data, result.data.data(),
               result.data.size() * sizeof(double));
      }

      valueStack.push(new Value(result.shape, raw_data));
      if (raw_data)
        free(raw_data); // Assuming Value makes a copy. If Value takes
                        // ownership, remove this free.
      return;

    } catch (...) {
      // Fallthrough to string handling if array parsing fails
    }
  }

  // 2. Try to parse as simple Number
  try {
    size_t pos;
    double num = std::stod(trimmed, &pos);
    if (pos == trimmed.length()) {
      valueStack.push(new Value(num));
      return;
    }
  } catch (...) {
    // Fallthrough
  }

  // 3. Fallback: Treat as String
  size_t num_doubles = (trimmed.length() + 1) / 2 + 1;
  double *data = (double *)malloc(sizeof(double) * num_doubles);
  memset(data, 0, sizeof(double) * num_doubles);
  memcpy(data, trimmed.c_str(), trimmed.length());

  std::vector<long> shape = {static_cast<long>(trimmed.length())};
  valueStack.push(new Value(shape, data));
  free(data);
}

} // extern "C"