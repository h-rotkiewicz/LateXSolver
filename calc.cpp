#include "utils.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr std::size_t MAX_LINE_LENGTH = 100;

constexpr auto PATH_TO_SCRIPT = "~/projects/scripts/Calculator/src/EvaluateExpression.Wls \"{}\"";

enum class COLOR {

  DEFAULT = 39,
  RED = 31,
  GREEN = 32,
  YELLOW = 33,
  CYAN = 36,
};

std::ostream &operator<<(std::ostream &os, COLOR color) {
  int code = static_cast<int>(color);
  return os << "\033[1;" << code << "m";
}

const char delimiter = '(';
const char limiter = ')';
const int MAX_ARG_LENGTH = 1000;

// equation needs to have double backslashes
std::string prepareString(std::string const &s) {
  std::string ret;
  for (auto c : s) {
    if (c == '\\') {
      ret.push_back('\\');
    }
    ret.push_back(c);
  }
  return ret;
};

std::string evaluateLaTeX(std::string const &equation) {
  std::array<char, 128> buffer;
  std::string result;
  auto eq = prepareString(equation);
  eq = std::format(PATH_TO_SCRIPT, eq);
  FILE *pipe = popen(eq.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  // Slow way to propagate down the stack, but I don't care
  if (result.find("Syntax") != std::string::npos ||
      result.find("Error") != std::string::npos ||
      // result.find("text") != std::string::npos ||
      result.find("Failed") != std::string::npos) {
    throw std::runtime_error("Syntax Error: " + equation);
  }
  // remove endline from result
  result.erase(result.find('\n'), 1);
  std::cout << COLOR::YELLOW << "Evaluated: " << COLOR::DEFAULT << equation
            << " = " << COLOR::CYAN << result << COLOR::DEFAULT << "\n";
  pclose(pipe);
  return result;
};

class buffer {
  std::vector<char> _buffer{};
  int _op_lenght{};
  int _current_index{};

public:
  buffer(int op) : _op_lenght(op) { _buffer.resize(_op_lenght); }

  void writeOP(char c) {
    if (_current_index >= _op_lenght) {
      _current_index = 0;
    }
    _buffer[_current_index++] = c;
  }
  std::string readOP() {
    std::string s{};
    for (int i = _current_index; i < _op_lenght; ++i) {
      s.push_back(_buffer[i]);
    }
    for (int i = 0; i < _current_index; ++i) {
      s.push_back(_buffer[i]);
    }
    // debug
    //  std::cout << "buffer read: " << s << "\n";
    return s;
  }
};

std::string get_ARG(std::stringstream &inputStream, char delimiter = '(',
                    char limiter = ')') {
  char c;
  int checker = 0;
  std::string ret;
  inputStream.unget();
  while ((c = inputStream.get()) != EOF) {
    if (ret.length() > MAX_ARG_LENGTH) {
      std::cerr << COLOR::RED << "Error: argument too long" << COLOR::DEFAULT
                << std::endl;
      return "";
    }
    if (c == delimiter) {
      checker++;
    } else if (c == limiter) {
      checker--;
    }
    if (!checker) {
      break;
    }
    // remove only the first the delimiter and limiter from the string
    // () chars are permitted in the argument
    if ((c != delimiter || checker != 1) && (c != limiter || checker != 0)) {
      ret.push_back(c);
    }
  }
  return ret;
};

template <typename processOP> class operations {
  processOP _op;
  std::string _op_name{};
  int _count{};

public:
  operations(processOP op, std::string const &op_name)
      : _op(op), _op_name(op_name), _count(op_name.length()) {}

  std::string expandOP(std::string const &s) {
    std::stringstream inputStream(s);
    buffer buff(_op_name.size());
    char c;
    std::stringstream ret;
    while ((c = inputStream.get()) != EOF) {
      if (c == delimiter && buff.readOP() == _op_name) {
        // remove the operator and substitute with the result
        ret.seekp(-_count, std::ios_base::end);
        ret << _op(get_ARG(inputStream));
      } else {
        buff.writeOP(c);
        ret.put(c);
      }
    }
    return ret.str();
  }
};

class VariableManager {

  std::unordered_map<std::string, std::string> _variables{};
  std::size_t _variables_count{};
  std::size_t _variables_changed{};

public:
  auto contains(std::string const &var) const -> bool {
    return _variables.contains(var);
  }

  auto GetValue(std::string const &Key) const -> std::string {
    if (contains(Key)) {
      return _variables.at(Key);
    }
    return "";
  }

  auto GetVariable(std::stringstream ss)
      -> std::pair<std::string, std::string> {
    std::vector<std::string> segments; // segments as in strings between "="
    std::string segment;
    while (std::getline(ss, segment, '=')) {
      segments.push_back(segment);
    }

    if (segments.size() >= 2 &&
        rules::variable_checker(trim(segments.front()))) {
      // remove \[ and \] from the segments
      return std::make_pair(trim(segments.front()), trim(segments.back()));
    }
    return std::make_pair("", "");
  }

  // TODO:
  // Discover new variables as the line is evaluated not the entire file at once
  auto Detect_variables(std::string line) {
    auto var = GetVariable(std::stringstream(line));
    if (!var.first.empty() && !var.second.empty()) {
      if (_variables.contains(var.first)) {
        std::cout << "Variable " << var.first << " changed from "
                  << _variables[var.first] << " to " << var.second << '\n';
      } else {
        _variables_changed++;
      }
      // std::cout << "Variable " << var.first << " = " << var.second << '\n';
      _variables[var.first] = var.second;
      _variables_count = _variables.size();
    }
  }

  void reset_counter() { _variables_changed = 0; }

  void print_variables() const {
    std::cout << "Recoginized variables: \n";
    for (auto const &[key, val] : _variables) {
      std::cout << COLOR::YELLOW << key << COLOR::DEFAULT << " = " << val
                << COLOR::DEFAULT << "\n";
    }
  }
};

class Writer {
  std::fstream _ofile;
  std::ifstream _ifile;
  std::string _ofilename;

public:
  Writer(std::string const &ifilename) {
    _ifile.open(ifilename);
    _ofile.open(_ofilename = ifilename + ".out");
    if(!_ofile.is_open()) {
      throw std::runtime_error("Error: can't open output file");
    }
    if (!_ifile.is_open()) {
      throw std::runtime_error("Error: file not found");
    }
    _ofile.clear();
  }

  constexpr auto Helper(std::string const &line) -> std::string { return line; }

  template <typename OP, typename... OPs>
  auto Helper(std::string const &line, OP &&op, OPs &&...ops) {
    return op.expandOP(Helper(line, ops...)); // Recursive case
  }

  /*Kinda a mess, TODO: fix it up, make it shiny.
   * main function of the program, generally once the \[line\] has been
   * extracted the plan is as follows:
   *  1. Try to substitute known variables, and expand the line to so the
   * operator can be evaluated
   *  2. Evaluate a line, meaning expand the operator and replace it with the
   * result
   *  3. If 2. fails, remember the line and skip it //For now, just write the
   * ARG as is, later on we can add a feature to remember the line and try to
   * evaluate it again after all the other lines have been evaluated
   *  4. Scan the line for new Varaibles, and add them to the list of known
   * variables
   */
  template <typename OP, typename... OPs>
  auto analyze(VariableManager &VM, OP op, OPs &...ops) {
    _ifile.seekg(0);
    _ifile.clear();
    std::string const lim = "\\[";
    std::string const delim = "\\]";
    std::string c;
    std::string line;
    int checker = 0;
    while (std::getline(_ifile, c, '\n')) {
      auto startpos = c.find(lim);
      auto endpos = c.rfind(delim);
      if (endpos == std::string::npos && startpos == std::string::npos) {
        _ofile << c << "\n";
        continue;
      }
      if (startpos != std::string::npos && endpos == std::string::npos) {
        line += c.substr(startpos + lim.length());
        checker--;
      } else if ((endpos) != std::string::npos &&
                 startpos == std::string::npos) {
        line += c.substr(0, endpos);
        checker++;
      } else if (endpos != std::string::npos && startpos != std::string::npos) {
        line = c.substr(startpos + lim.length(),
                        endpos - (startpos + lim.length()));
        checker = 0;
      }
      if (checker) {
        continue;
      }
      if (checker < 0) {
        throw std::runtime_error("FUCKED UP \\[\\]" + lim + " and " + delim);
      }
      std::stringstream ss(line);
      try {
        auto EvaluatedLine = op.expandOP(Helper(line, ops...));
        VM.Detect_variables(EvaluatedLine);
        EvaluatedLine = lim + EvaluatedLine + delim;
        _ofile << EvaluatedLine << "\n";
      } catch (...) {
        std::cout << COLOR::RED << "Can't evaluate " << line << ", skipping..."
                  << COLOR::DEFAULT << "\n";
        _ofile << lim << line << delim;
      }
      line = "";
    }
    VM.reset_counter();
    VM.print_variables();
  }
};


int main(int argc, char *argv[]) {
  VariableManager VM;
  auto CALCULATE = [](std::string const &arg) -> std::string {
    return arg + " = " + evaluateLaTeX(arg);
    // return arg + " = 0"; // for testing
  };

  auto SUBSTITUTE = [&VM](std::string const &arg)
      -> std::string { // IDK about this, both the lambda and the writer modify
                       // the same VM.
    std::string Result = arg + ' ';
    std::string delimiter = " ,\n\t()=;\\+-/%*^\'\"";
    bool NotSubstituted = true;
    size_t pos_start = 0;
    std::vector<std::string> res;
    for (auto pos_end = 0; pos_end < Result.length(); ++pos_end) {
      if (delimiter.find(Result[pos_end]) == std::string::npos) {
        continue;
      }
      auto token = Result.substr(pos_start, pos_end - pos_start);
      // std::cout << "Token: " << token << "\n";
      // std::cout << "In line: " << Result << "\n";
      // std::cout << "Pos start: " << pos_start << " Pos end: " << pos_end
                // << "\n";
      pos_start = pos_end + 1;
      if (token.empty()) {
        continue;
      }
      // std::cout << "Token is in VM: " << VM.contains(trim(token)) << "\n";
      if (VM.contains(trim(token))) {
        NotSubstituted = false;
        std::string val = VM.GetValue(trim(token));
        Result.replace(Result.find(token), token.length(), val);
        // std::cout << "Substituted " << token << " with " << val << "\n";
      }
      res.push_back(token);
    }
    Result = trim(Result);
    if (NotSubstituted) {
      return "CALC(" + arg + ")";
    }
    return arg + " = CALC(" + Result +
           ")"; // TODO: REPLACE 2 + 3 with substitute function
  };
  try {

    operations<decltype(CALCULATE)> CALC(CALCULATE, "CALC");
    operations<decltype(SUBSTITUTE)> SUB(SUBSTITUTE, "CALC");
    Writer writer(argv[1]);
    writer.analyze(VM, CALC, SUB);

  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
