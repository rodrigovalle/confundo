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

  try {
    char* data;
    UDPSocket sock(argv[1]);
    sock.recv(&data);
    std::cout << data << std::endl;
    return EXIT_SUCCESS;
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
