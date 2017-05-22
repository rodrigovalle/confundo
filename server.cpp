#include "cfp.hpp"
#include "udpsocket.hpp"
#include "udpmux.hpp"

#include <cstdlib>  // EXIT_*
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "./server <PORT> <FILE-DIR>" << std::endl;
    return EXIT_FAILURE;
  }

  std::string port{argv[1]};
  std::string filedir{argv[2]};
  std::vector<CFP> protocol;

  try {
    uint8_t data[MAXPACKET];
    size_t size;
    struct sockaddr_in addr;
    uint64_t id = 1;

    UDPSocket udpsock = UDPSocket::bind(port);
    UDPMux muxer{udpsock};

    while (true) {
      size = udpsock.recvfrom(data, MAXPACKET, &addr);
      try {
        muxer.deliver(&addr, data, size);
      } catch (std::out_of_range& e) {
        protocol.emplace_back(muxer, id);
        muxer.connect(&protocol.back(), &addr);
        muxer.deliver(&addr, data, size);
      }
      id++;
    }

    return EXIT_SUCCESS;

  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
