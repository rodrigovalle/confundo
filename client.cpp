#include "cfp.hpp"
#include "udpmux.hpp"
#include "udpsocket.hpp"

#include <cstdlib>  // EXIT_*
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string
#include <fstream>

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

  std::string hostname{argv[1]};
  std::string port{argv[2]};
  std::ifstream file{argv[3]};

  try {
    uint8_t data[MAXPACKET];
    struct sockaddr_in addr;
    size_t size;
    UDPSocket udpsock = UDPSocket::bind("0");
    UDPMux mux{udpsock};

    PayloadT buf;
    file.read(reinterpret_cast<char*>(buf.first.data()), 512);
    buf.second = file.gcount();
    CFP cfp{mux, hostname, port, buf};

    while (true) {
      size = udpsock.recvfrom(data, MAXPACKET, &addr);
      try {
        mux.deliver(&addr, data, size);
      } catch (std::out_of_range& e) {
        std::cerr << "received packet not from server" << std::endl;
      }

      if (!file.eof()) {
        do {
          file.read(reinterpret_cast<char*>(buf.first.data()), 512);
        } while (cfp.send(buf) && !file.eof());

        if (file.eof()) {
          cfp.close();
        }
      }
    }
    return EXIT_SUCCESS;

  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
