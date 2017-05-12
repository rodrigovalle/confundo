#include "udpsocket.hpp"
#include <string>

class ConfundoSocket {
 public:
  ConfundoSocket(const std::string& host, const std::string& port);
  explicit ConfundoSocket(const std::string& port);

  void send_all(const std::string& data);
  std::string receive();

 private:
  void connect();
  void listen();
  UDPSocket sock;
};
