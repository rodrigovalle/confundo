#include "cfp.hpp"
#include "eventloop.hpp"
#include "udpsocket.hpp"
#include "util.hpp"

#include <csignal>
#include <cstdlib>  // EXIT_*
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string
#include <unistd.h>

static void handle_signals(int signum) {
  _exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "./server <PORT> <FILE-DIR>" << std::endl;
    return EXIT_FAILURE;
  }

  signal(SIGINT, handle_signals);
  signal(SIGQUIT, handle_signals);
  signal(SIGTERM, handle_signals);

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
        CFP new_cfp{/*XXX: UDPSocket*/, filedir, id};
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
