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

void UDPMux::disconnect(CFP* proto) {
  auto addrpair = unpack_sockaddr(&mux[proto]);

  for (auto i = mux.begin(); i != mux.end(); i++) {
    if (i->first == proto) {
      mux.erase(i);
      break;
    }
  }

  for (auto i = demux.begin(); i != demux.end(); i++) {
    if (i->first.first == addrpair.first && i->first.second == addrpair.second) {
      demux.erase(i);
      break;
    }
  }
}

void UDPMux::send(CFP* proto, const uint8_t data[], size_t size) const {
  sock.sendto(data, size, &mux.at(proto));
}

void UDPMux::deliver(const sockaddr_in* addr, uint8_t data[], size_t size) const {
  CFP* proto = demux.at(unpack_sockaddr(addr));
  proto->recv_event(data, size);
}

std::pair<uint32_t, uint16_t> UDPMux::unpack_sockaddr(const sockaddr_in* addr) const {
  uint16_t port;
  uint32_t ipaddr;

  port = ntohs(addr->sin_port);
  ipaddr = ntohl(addr->sin_addr.s_addr);

  return std::make_pair(ipaddr, port);
}
