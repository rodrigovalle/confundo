#include "udpconnection.hpp"
#include "util.hpp"

#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UDPConnection::UDPConnection(const UDPSocket& socket, struct sockaddr peeraddr,
                             socklen_t peeraddrlen)
    : sock{socket}, addr{peeraddr}, addrlen{peeraddrlen} {}

UDPConnection::UDPConnection(const UDPSocket& socket, const std::string& host,
                             const std::string& port) : sock{socket} {
  struct addrinfo hints = {}; // zero initialize struct
  struct addrinfo* result;
  int e;

  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_DGRAM;  // UDP

  e = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
  if (e != 0) {
    throw std::runtime_error{"getaddrinfo(): " + std::string(gai_strerror(e))};
  }

  addr = *(result->ai_addr);
  addrlen = result->ai_addrlen;
  freeaddrinfo(result);
}

void UDPConnection::send(const uint8_t data[], size_t size) const {
  sock.sendto(data, size, &addr, addrlen);
}

size_t UDPConnection::recv(uint8_t data[], size_t size) {
  struct sockaddr addr;
  socklen_t addrlen;

  return sock.recvfrom(data, size, &addr, &addrlen);
}
