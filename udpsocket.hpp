#ifndef UDPSOCKET_HPP
#define UDPSOCKET_HPP

#include <string>
#include <sys/socket.h>

class UDPConnection;

class UDPSocket {
 public:
  UDPSocket(const UDPSocket& other) = delete;
  UDPSocket(UDPSocket&& other) noexcept;
  ~UDPSocket();

  UDPSocket& operator=(const UDPSocket& other) = delete;
  UDPSocket& operator=(UDPSocket&& other) noexcept;

  static UDPSocket bind(const std::string& port);

  void sendto(const uint8_t data[], size_t size, const struct sockaddr* addr,
              socklen_t addrlen) const;
  size_t recvfrom(uint8_t data[], size_t size, struct sockaddr* addr,
                  socklen_t* addrlen) const;

  void register_connection(UDPConnection& conn);

 private:
  explicit UDPSocket(int fd);
  int sockfd;
};

#endif // UDPSOCKET_HPP
