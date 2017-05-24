#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <vector>

class UDPMux;
class CFP;

struct cf_header;
enum op_T {
  SEND,
  RECV,
  DROP
};

inline std::string mkerrorstr(const std::string& fn_name) {
  return std::string(fn_name + ": " + strerror(errno));
}

/* reports a packet event to std::cout */
void report(op_T op, const cf_header* hdr, uint32_t cwnd, uint32_t ssthresh,
            bool duplicate);
sockaddr_in getsockaddr(const std::string& host, const std::string& port);

void host_to_net(struct cf_header* hdr);
void net_to_host(struct cf_header* hdr);

#endif  // _UTIL_HPP
