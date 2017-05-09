#include "udpsocket.hpp"

class ConfundoSocket {
 public:
  ConfundoSocket(const std::string& host, const std::string& port);
  explicit ConfundoSocket(const std::string& port);

  void send_all(const std::string& data);
  void receive();

 private:
  void connect();
  void listen();
};
