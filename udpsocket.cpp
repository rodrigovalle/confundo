#include "udpsocket.hpp"
#include "util.hpp"

#include <stdexcept>     // std::runtime_error
#include <string>        // std::string

#include <sys/types.h>
#include <sys/socket.h>  // send/recv, socket
#include <netdb.h>       // getaddrinfo
#include <unistd.h>      // close


/* client constructor */
UDPSocket::UDPSocket(const std::string& host, const std::string& port) {
  struct addrinfo hints{}; // zero initialize struct
  struct addrinfo* result;
  struct addrinfo* rp;
  std::string cause;
  int err;

  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_DGRAM;  // UDP

  err = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
  if (err != 0) {
    throw std::runtime_error{"getaddrinfo(): " +
                             std::string(gai_strerror(err))};
  }

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      cause = mkerrorstr("socket");
      continue;
    }

    if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      cause = mkerrorstr("connect");
      close(sockfd);
      continue;
    }

    break;  // we got a live one
  }

  freeaddrinfo(result);
  if (rp == nullptr) {
    throw std::runtime_error{cause};
  }
}


/* server constructor */
UDPSocket::UDPSocket(const std::string& port) {
  struct addrinfo hints{};
  struct addrinfo* result;
  struct addrinfo* rp;
  std::string cause;
  int err;

  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_DGRAM;  // UDP
  hints.ai_flags = AI_PASSIVE;     // use my IP

  err = getaddrinfo("0.0.0.0", port.c_str(), &hints, &result);
  if (err != 0) {
    throw std::runtime_error{"getaddrinfo(): " +
                             std::string(gai_strerror(err))};
  }

  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      cause = mkerrorstr("socket");
      continue;
    }

    if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
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
}


UDPSocket::~UDPSocket() {
  close(sockfd);
}


void UDPSocket::send(const uint8_t data[], size_t size) {
  /* UDP send will either succeed completely, or fail
   * see: http://stackoverflow.com/questions/43746020 */
  if (::send(sockfd, data, size, 0) == -1) {
    throw std::runtime_error{mkerrorstr("send")};
  }
}


size_t UDPSocket::recv(uint8_t data[], size_t size) {
  ssize_t recv_b;

  if ((recv_b = ::recv(sockfd, data, size, 0)) == -1) {
    throw std::runtime_error{mkerrorstr("recv")};
  }

  return recv_b;
}

size_t UDPSocket::recv_connect(uint8_t data[], size_t size) {
  struct sockaddr addr;
  socklen_t addrlen;
  ssize_t recv_b;

  if ((recv_b = ::recvfrom(sockfd, data, size, 0, &addr, &addrlen)) == -1) {
    throw std::runtime_error{mkerrorstr("recvfrom")};
  }

  if (::connect(sockfd, &addr, addrlen) == -1) {
    throw std::runtime_error{mkerrorstr("connect")};
  }

  return recv_b;
}
