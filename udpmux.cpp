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

void UDPMux::connect(CFP* proto, sockaddr* addr, socklen_t addrlen) {
  mux.insert({proto, {*addr, addrlen}});
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

  sockaddr addr = *(result->ai_addr);
  socklen_t addrlen = result->ai_addrlen;
  freeaddrinfo(result);

  mux.insert({proto, {addr, addrlen}});
  demux.insert({unpack_sockaddr(&addr), proto});
}

void UDPMux::send(CFP* proto, const uint8_t data[], size_t size) const {
  auto addrpair = mux.at(proto);
  sock.sendto(data, size, &std::get<0>(addrpair), std::get<1>(addrpair));
}

void UDPMux::deliver(const sockaddr* addr, uint8_t data[], size_t size) const {
  CFP* proto = demux.at(unpack_sockaddr(addr));
  proto->event(data, size);
}

std::pair<uint16_t, uint64_t> UDPMux::unpack_sockaddr(const sockaddr* addr) const {
  uint16_t port;
  uint32_t ipaddr;
  const sockaddr_in* addr_in;

  switch (addr->sa_family) {
    case AF_INET:
      addr_in = reinterpret_cast<const sockaddr_in*>(addr);
      port = addr_in->sin_port;
      ipaddr = addr_in->sin_addr.s_addr;
      return std::make_pair(port, ipaddr);

    case AF_INET6:
    default:
      throw std::runtime_error{"this server doesn't support IPV6"};
  }
}
