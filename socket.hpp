class SendingSocket {
 public:
  SendingSocket(const std::string& host, const std::string& port);
  void send(const std::string& data);
 private:
  int sockfd;
};

class ReceivingSocket {
 public:
  ReceivingSocket(const std::string& port);
  std::string recv(unsigned int size);
 private:
  int sockfd;
};