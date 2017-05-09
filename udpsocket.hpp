#ifndef UDPSOCKET_HPP
#define UDPSOCKET_HPP

#include <string>

#define MAXUDPSIZE 524

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

  void send(const char* data, size_t size);
  size_t recv(char** data);

 private:
  int sockfd;
  char buf[MAXUDPSIZE];
};

#endif // UDPSOCKET_HPP
