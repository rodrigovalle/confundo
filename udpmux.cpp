#include "udpmux.hpp"
#include "cfp.hpp"
#include "udpsocket.hpp"
#include "util.hpp"

#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UDPMux::UDPMux(const UDPSocket& udpsock) : sock{udpsock} {}
UDPMux::~UDPMux() {}

void UDPMux::connect(CFP* proto, sockaddr_in* addr) {
  mux.insert({proto, *addr});
  demux.insert({unpack_sockaddr(addr), proto});
}

void UDPMux::connect(CFP* proto, const std::string& host,
                     const std::string& port) {
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

  mux.insert({proto, addr});
  demux.insert({unpack_sockaddr(&addr), proto});
}

void UDPMux::send(CFP* proto, const uint8_t data[], size_t size) const {
  sock.sendto(data, size, &mux.at(proto));
}

void UDPMux::deliver(const sockaddr_in* addr, uint8_t data[], size_t size) const {
  CFP* proto = demux.at(unpack_sockaddr(addr));
  proto->event(data, size);
}

std::pair<uint32_t, uint16_t> UDPMux::unpack_sockaddr(const sockaddr_in* addr) const {
  uint16_t port;
  uint32_t ipaddr;

  port = ntohs(addr->sin_port);
  ipaddr = ntohl(addr->sin_addr.s_addr);

  return std::make_pair(ipaddr, port);
}
