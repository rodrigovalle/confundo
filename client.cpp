#include "cfp.hpp"
#include "udpsocket.hpp"
#include "util.hpp"
#include "eventloop.hpp"

#include <csignal>
#include <cstdlib>  // EXIT_*
#include <fstream>
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string
#include <unistd.h>

static void handle_signals(int signum) {
  _exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cout << "./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
    return EXIT_FAILURE;
  }

  signal(SIGINT, handle_signals);
  signal(SIGQUIT, handle_signals);
  signal(SIGTERM, handle_signals);

  std::string hostname{argv[1]};
  std::string port{argv[2]};
  std::ifstream file{argv[3]};

  // XXX: in this order
  //  - use util.hpp connect on host, port
  //  - use EventLoop::add on cfp_object
  //  - use cfp::start on cfp_object to send syn
  try {
    // read data for first packet
    PayloadT buf;
    file.read(reinterpret_cast<char*>(buf.first.data()), 512);
    buf.second = file.gcount(); // set number of bytes read

    // initialize event loop
    sockaddr_in addr = getsockaddr(hostname, port);
    EventLoop evloop{UDPSocket::bind("0")};

    // add CFP object to eventloop and make it send syn by calling start()
    CFP& cfp = evloop.add(CFP{evloop.getmux(), buf}, &addr);
    cfp.start();

    enum {
      OK,
      RETRY_LAST_SEND,
      FILE_EOF
    } send_state = OK;

    while (true) {
      try {
        evloop.run();
      } catch (delivery_exception& ex) {
        throw std::runtime_error{"receieved packet that wasn't from the server"};
      }

      // ugly state machine to deal with resending failed sends and file eof
      switch (send_state) {
        case RETRY_LAST_SEND:
          if (!cfp.send(buf)) {
            std::cerr << "failed to send" << std::endl;
            break; // failed to send, try again next time kiddo
          } // else, fall to next case and send until we can't anymore

          if (file.eof()) { // hit the end of the file
            std::cerr << "hit EOF, closing file" << std::endl;
            cfp.close();
            send_state = FILE_EOF;
            break;
          }

        case OK:
          while (!file.eof()) {
            file.read(reinterpret_cast<char*>(buf.first.data()), 512);
            buf.second = file.gcount();

            if (!cfp.send(buf)) {
              send_state = RETRY_LAST_SEND;
              break;
            }
          }
          break;

        case FILE_EOF:
          // no more data to transmit, but continue delivering cfp packets
          break;
      }
    }

  } catch (connection_closed_gracefully& e) {
    return EXIT_SUCCESS;
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
