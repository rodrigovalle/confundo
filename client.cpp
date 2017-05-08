#include "udpsocket.hpp"

#include <string>
#include <thread>
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cout << "./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
    return EXIT_FAILURE;
  }

  try {
    UDPSocket sock(argv[1], argv[2]);
    sock.send("hello", 5);
    return EXIT_SUCCESS;
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
