#include "util.hpp"
#include "cfp.hpp"
#include "timer.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

void report(op_T op, const cf_header* hdr, uint32_t cwnd, uint32_t ssthresh,
            bool duplicate) {
  switch (op) {
    case SEND:
      std::cout << "SEND";
      break;
    case RECV:
      std::cout << "RECV";
      break;
    case DROP:
      std::cout << "DROP";
      break;
  }

    std::cout << " " << hdr->seq;
    std::cout << " " << hdr->ack;
    std::cout << " " << hdr->conn;

  if (cwnd != 0 || ssthresh != 0) {
    std::cout << " " << cwnd;
    std::cout << " " << ssthresh;
  }

  if (hdr->ack_f) {
    std::cout << " ACK";
  }
  if (hdr->syn_f) {
    std::cout << " SYN";
  }
  if (hdr->fin_f) {
    std::cout << " FIN";
  }
  if (duplicate) {
    std::cout << " DUP";
  }
  std::cout << std::endl;
}

sockaddr_in getsockaddr(const std::string& host, const std::string& port) {
  struct addrinfo hints = {}; // zero initialize struct
  struct addrinfo* result;
  int e;

  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_DGRAM;  // UDP

  e = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
  if (e != 0) {
    throw std::runtime_error{"getaddrinfo(): " + std::string(gai_strerror(e))};
  }

  sockaddr_in addr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
  freeaddrinfo(result);

  return addr;
}

// convert to network byte order
void host_to_net(struct cf_header* hdr) {
  hdr->seq = htonl(hdr->seq);
  hdr->ack = htonl(hdr->ack);
  hdr->conn = htons(hdr->conn);
  hdr->flgs = htons(hdr->flgs);
}

// convert from network byte order
void net_to_host(struct cf_header* hdr) {
  hdr->seq = ntohl(hdr->seq);
  hdr->ack = ntohl(hdr->ack);
  hdr->conn = ntohs(hdr->conn);
  hdr->flgs = ntohs(hdr->flgs);
}
