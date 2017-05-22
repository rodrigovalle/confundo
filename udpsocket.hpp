#ifndef _UDPSOCKET_HPP
#define _UDPSOCKET_HPP

#include <string>
#include <sys/socket.h>

class UDPSocket {
 public:
  UDPSocket(const UDPSocket& other) = delete;
  UDPSocket(UDPSocket&& other) noexcept;
  ~UDPSocket();

  UDPSocket& operator=(const UDPSocket& other) = delete;
  UDPSocket& operator=(UDPSocket&& other) noexcept;

  static UDPSocket bind(const std::string& port);

  void sendto(const uint8_t data[], size_t size, const struct sockaddr_in* addr) const;
  size_t recvfrom(uint8_t data[], size_t size, struct sockaddr_in* addr) const;

 private:
  explicit UDPSocket(int fd);
  int sockfd;
};

#endif // _UDPSOCKET_HPP
