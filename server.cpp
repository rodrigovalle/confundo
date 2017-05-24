#include "cfp.hpp"
#include "eventloop.hpp"
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

  if (filedir.back() != '/') {
    filedir += '/';
  }

  try {
    uint16_t id = 1;

    EventLoop evloop{UDPSocket::bind(port)};

    while (true) {
      try {
        evloop.run();
      } catch (delivery_exception& ex) {
        CFP new_cfp{evloop.getmux(), id, filedir};
        evloop.add(std::move(new_cfp), ex.addr).recv_event(ex.data, ex.size);
        id++;
      } catch (connection_closed& e) {
        evloop.remove(e.which);
      }
    }

    return EXIT_SUCCESS;

  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
