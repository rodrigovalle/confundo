#include "socket.hpp"

#include <sys/types.h>   // getaddrinfo
#include <sys/socket.h>  // getaddrinfo
#include <netdb.h>       // getaddrinfo

#include <string>        // std::string
#include <stdexcept>     // std::runtime_error

SendingSocket::SendingSocket(const std::string& host,
                             const std::string& port) {
  struct addrinfo hints{0};
  struct addrinfo* res;
  int err;

  hints.ai_family = ;
  hints.ai_socktype = ;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_flags = ;

  err = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (err != 0) {
    raise std::runtime_error("getaddrinfo(): " +
                             std::string(gai_strerror(err)));
  }

  freaddrinfo(res);
}
