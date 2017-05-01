#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <string>
#include <cerrno>
#include <cstring>

std::string mkerrorstr(std::string fn_name) {
  return std::string(fn_name + ": " + strerror(errno));
}

#endif  // _UTIL_HPP
