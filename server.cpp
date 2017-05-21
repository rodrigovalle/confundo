#include "confundosocket.hpp"
#include "udpsocket.hpp"

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

  try {
    UDPSocket udpsock = UDPSocket::bind(port);
    ConfundoSocket cfsock = ConfundoSocket::accept(udpsock, 0);
    std::string r = cfsock.receive();
    std::cout << r << std::endl;
    return EXIT_SUCCESS;
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
