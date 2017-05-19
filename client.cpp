#include "confundosocket.hpp"

#include <cstdlib>  // EXIT_*
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string

static char test[] = "iLorem ipsum dolor sit amet, consectetur adipiscing "
  "elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
  "enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
  "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
  "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
  "occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit "
  "anim id est laborum.";

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cout << "./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
    return EXIT_FAILURE;
  }

  try {
    ConfundoSocket sock(argv[1], argv[2]);
    std::cout << "handshake successful" << std::endl;
    sock.send_all(test);
    return EXIT_SUCCESS;
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
