#include "udpsocket.hpp"
#include "util.hpp"

#include <stdexcept>     // std::runtime_error
#include <string>        // std::string

#include <netdb.h>       // getaddrinfo
#include <sys/socket.h>  // sendto/recvfrom, socket
#include <sys/types.h>
#include <unistd.h>      // close


UDPSocket::UDPSocket(int fd) : sockfd{fd} {}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept {
  sockfd = other.sockfd;
  other.sockfd = -1;
}

/* server constructor */
UDPSocket UDPSocket::bind(const std::string& port) {
  struct addrinfo hints{};
  struct addrinfo* result;
  struct addrinfo* rp;
  std::string cause;
  int sockfd, e;

  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_DGRAM;  // UDP
  hints.ai_flags = AI_PASSIVE;     // use my IP

  e = getaddrinfo("0.0.0.0", port.c_str(), &hints, &result);
  if (e != 0) {
    throw std::runtime_error{"getaddrinfo(): " + std::string(gai_strerror(e))};
  }

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      cause = mkerrorstr("socket");
      continue;
    }

    if (::bind(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      cause = mkerrorstr("bind");
      close(sockfd);
      sockfd = -1;
      continue;
    }

    break; // we caught a live one
  }

  freeaddrinfo(result);
  if (rp == nullptr) {
    throw std::runtime_error{cause};
  }

  return UDPSocket{sockfd};
}

UDPSocket::~UDPSocket() {
  close(sockfd);
}

void UDPSocket::sendto(const uint8_t data[], size_t size,
                       const struct sockaddr_in* addr) const {
  /* UDP send will either succeed completely, or fail
   * see: http://stackoverflow.com/questions/43746020 */
  if (::sendto(sockfd, data, size, 0, reinterpret_cast<const sockaddr*>(addr),
               sizeof(sockaddr_in)) == -1) {
    throw std::runtime_error{mkerrorstr("sendto")};
  }
}

size_t UDPSocket::recvfrom(uint8_t data[], size_t size,
                           struct sockaddr_in* addr) const {
  ssize_t recv_b;
  socklen_t len = sizeof(struct sockaddr_in);

  if ((recv_b = ::recvfrom(sockfd, data, size, 0,
                           reinterpret_cast<sockaddr*>(addr), &len)) == -1) {
    throw std::runtime_error{mkerrorstr("recvfrom")};
  }

  return recv_b;
}
