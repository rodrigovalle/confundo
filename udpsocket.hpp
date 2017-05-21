#ifndef UDPSOCKET_HPP
#define UDPSOCKET_HPP

#include <string>

/* TODO: implement copy/move constructor/assignment? */
class UDPSocket {
 public:
  UDPSocket(const std::string& host, const std::string& port);  // client
  explicit UDPSocket(const std::string& port);  // server
  UDPSocket(const UDPSocket& other) = delete;
  UDPSocket(const UDPSocket&& other) = delete;
  ~UDPSocket();

  UDPSocket& operator=(const UDPSocket& other) = delete;
  UDPSocket& operator=(const UDPSocket&& other) = delete;

  /* it's up to the user to make sure that the buffer has enough space */
  void send(const uint8_t data[], size_t size);
  size_t recv(uint8_t data[], size_t size);
  size_t recv_connect(uint8_t data[], size_t size);

 private:
  int sockfd;
};

#endif // UDPSOCKET_HPP
