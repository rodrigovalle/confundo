#ifndef _UDPSOCKET_HPP
#define _UDPSOCKET_HPP

#include <string>

#define MAXUDPSIZE 524

class UDPSocket {
 public:
  UDPSocket(const std::string& host, const std::string& port);  // client
  UDPSocket(const std::string& port);  // server
  ~UDPSocket();
  void send(const char* data, size_t size);
  size_t recv(char** data);

 private:
  int sockfd;
  char buf[MAXUDPSIZE];
};

#endif  // _UDPSOCKET_HPP
