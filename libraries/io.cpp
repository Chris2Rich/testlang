#include "../stack_runtime.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <time.h>
#include <vector>

// Parse a token line to extract value and type
// Format: Token(value='...', type=<TokenType.XXX: N>)
// or: Token(value=([shape], [data]), type=<TokenType.ARR: 3>)
struct ParsedToken {
  std::string value;
  std::string type;
};

ParsedToken parse_token_line(const std::string &line) {
  ParsedToken result;

  // Match Token(value='...', type=<TokenType.XXX: N>)
  std::regex simpleRegex(
      R"(Token\(value='([^']*)',\s*type=<TokenType\.(\w+):\s*\d+>\))");
  // Match Token(value=([...], [...]), type=<TokenType.ARR: 3>)
  std::regex arrayRegex(
      R"(Token\(value=\((\[[^\]]*\]),\s*(\[[^\]]*\])\),\s*type=<TokenType\.(\w+):\s*\d+>\))");

  std::smatch match;
  if (std::regex_match(line, match, arrayRegex)) {
    // ARR token: shape is match[1], data is match[2]
    result.value = match[1].str() + "|" + match[2].str();
    result.type = match[3].str();
  } else if (std::regex_match(line, match, simpleRegex)) {
    result.value = match[1].str();
    result.type = match[2].str();
  }

  return result;
}

// Parse shape string like "[2, 2]"
std::vector<long> parse_shape(const std::string &shapeStr) {
  std::vector<long> shape;
  std::string content = shapeStr.substr(1, shapeStr.length() - 2); // Remove [ ]
  std::stringstream ss(content);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Trim whitespace
    size_t first = item.find_first_not_of(" \t");
    if (first == std::string::npos)
      continue;
    size_t last = item.find_last_not_of(" \t");
    std::string trimmed = item.substr(first, last - first + 1);

    if (!trimmed.empty()) {
      shape.push_back(std::stol(trimmed));
    }
  }

  return shape;
}

// Parse data string like "[1.0, 2.0, 3.0, 4.0]"
std::vector<double> parse_data(const std::string &dataStr) {
  std::vector<double> data;
  std::string content = dataStr.substr(1, dataStr.length() - 2); // Remove [ ]
  std::stringstream ss(content);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Trim whitespace
    size_t first = item.find_first_not_of(" \t");
    if (first == std::string::npos)
      continue;
    size_t last = item.find_last_not_of(" \t");
    std::string trimmed = item.substr(first, last - first + 1);

    if (!trimmed.empty()) {
      data.push_back(std::stod(trimmed));
    }
  }

  return data;
}

extern "C" {

// io.input - Read a line from stdin and parse using Python lexer
void input() {
  std::string line;
  if (!std::getline(std::cin, line)) {
    valueStack.push(new Value(0.0));
    return;
  }

  size_t first = line.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    valueStack.push(new Value(0.0));
    return;
  }
  size_t last = line.find_last_not_of(" \t\n\r");
  std::string trimmed = line.substr(first, last - first + 1);

  // Use clock_gettime for nanosecond precision to avoid same-second
  // collisions
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long buffer = (long)ts.tv_sec * 1000000000L + ts.tv_nsec;

  // Store as std::string
  std::string tempInput = std::string("/tmp/testlang_io_input")
                              .append(std::to_string(buffer))
                              .append(".tmp");
  std::string tempTokens = std::string("/tmp/testlang_io_tokens")
                               .append(std::to_string(buffer))
                               .append(".tmp");

  //  RAII cleanup guard — runs on every exit path
  auto cleanup = [&]() {
    std::remove(tempInput.c_str());
    std::remove(tempTokens.c_str());
  };

  std::ofstream ofs(tempInput);
  if (!ofs.is_open()) {
    valueStack.push(new Value(0.0));
    return; // nothing to clean yet
  }
  ofs << trimmed;
  ofs.close();

  std::string cmd = "python3 lexer.py ";
  cmd += tempInput;
  cmd += " ";
  cmd += tempTokens;
  cmd += " 2>/dev/null";

  int result = std::system(cmd.c_str());
  if (result != 0) {
    cleanup();
    valueStack.push(new Value(0.0));
    return;
  }

  std::ifstream tokenFile(tempTokens);
  if (!tokenFile.is_open()) {
    cleanup();
    valueStack.push(new Value(0.0));
    return;
  }

  std::string tokenLine;
  bool pushed = false;
  while (std::getline(tokenFile, tokenLine)) {
    ParsedToken tok = parse_token_line(tokenLine);

    if (tok.type == "NUM") {
      valueStack.push(new Value(std::stod(tok.value)));
      pushed = true;
      break;
    } else if (tok.type == "ARR") {
      size_t sep = tok.value.find('|');
      if (sep != std::string::npos) {
        std::string shapeStr = tok.value.substr(0, sep);
        std::string dataStr = tok.value.substr(sep + 1);

        std::vector<long> shape = parse_shape(shapeStr);
        std::vector<double> data = parse_data(dataStr);

        if (!shape.empty() && !data.empty()) {
          valueStack.push(new Value(shape, data.data()));
          pushed = true;
        } else if (shape.size() == 1 && shape[0] == 0) {
          valueStack.push(new Value(shape, nullptr));
          pushed = true;
        }
      }
      break;
    }
  }

  tokenFile.close();
  cleanup();

  if (!pushed) {
    valueStack.push(new Value(0.0));
  }
}

void output() {
  if (valueStack.empty()) {
    return;
  }

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    // Scalar: print single ASCII character
    long code = static_cast<long>(val->data[0]);
    std::cout << static_cast<char>(code);
  } else if (val->shape.size() == 1) {
    // 1D array: print each element as ASCII character (a string)
    for (long i = 0; i < val->shape[0]; ++i) {
      long code = static_cast<long>(val->data[i]);
      std::cout << static_cast<char>(code);
    }
  } else {
    // Tensor (2D+): error
    std::cerr << "Runtime Error: io.output does not support tensors." << std::endl;
  }

  delete val;
}
} // extern "C"
