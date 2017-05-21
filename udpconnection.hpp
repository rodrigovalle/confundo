#ifndef _UDP_CONNECTION
#define _UDP_CONNECTION

#include "udpsocket.hpp"
#include <sys/socket.h>

class UDPConnection {
 public:
  UDPConnection(const UDPSocket& socket, struct sockaddr peeraddr,
                socklen_t peeraddrlen);
  UDPConnection(const UDPSocket& socket, const std::string& host,
                const std::string& port);

  void send(const uint8_t data[], size_t size) const;
  size_t recv(uint8_t data[], size_t size);

 private:
  const UDPSocket& sock;
  struct sockaddr addr;
  socklen_t addrlen;
};

#endif // _UDP_CONNECTION
