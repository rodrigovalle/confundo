#ifndef _UTIL_HPP
#define _UTIL_HPP

#include "confundosocket.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

enum op_T {
  SEND,
  RECV,
  DROP
};

/* reports a packet event to std::cout */
void report(op_T op, const struct cf_header* hdr, uint32_t cwnd,
            uint32_t ssthresh);

inline std::string mkerrorstr(const std::string& fn_name) {
  return std::string(fn_name + ": " + strerror(errno));
}

#endif  // _UTIL_HPP
