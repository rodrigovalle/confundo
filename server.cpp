#include "udpsocket.hpp"
#include <string>
#include <thread>
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "./server <PORT> <FILE-DIR>" << std::endl;
    return EXIT_FAILURE;
  }

  try {
    UDPSocket sock(argv[1]);
    sock.recv();
  }
}
