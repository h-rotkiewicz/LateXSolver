#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


namespace rules { // namespace rules

// check if the string is a valid variable name
inline bool variable_checker(std::string const &s) {
  if (s.find(" ") != std::string::npos) {
    return false;
  }
  return true;
}

} // namespace rules

inline std::string &ltrim(std::string &s, const char *t = " \t\n\r\f\v") {
  s.erase(0, s.find_first_not_of(t));
  return s;
}

inline std::string &rtrim(std::string &s, const char *t = " \t\n\r\f\v") {
  s.erase(s.find_last_not_of(t) + 1);
  return s;
}

inline std::string &trim(std::string &s, const char *t = " \t\n\r\f\v") {
  return ltrim(rtrim(s, t), t);
}
