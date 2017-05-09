#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <cerrno>
#include <cstring>
#include <string>

inline std::string mkerrorstr(const std::string& fn_name) {
  return std::string(fn_name + ": " + strerror(errno));
}

#endif  // _UTIL_HPP
