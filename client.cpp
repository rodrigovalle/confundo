#include "cfp.hpp"
#include "udpmux.hpp"
#include "udpsocket.hpp"

#include <cstdlib>  // EXIT_*
#include <fstream>
#include <iostream> // std::cout, std::cerr
#include <string>   // std::string

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cout << "./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
    return EXIT_FAILURE;
  }

  std::string hostname{argv[1]};
  std::string port{argv[2]};
  std::ifstream file{argv[3]};

  // XXX: in this order
  //  - use util.hpp connect on host, port
  //  - use EventLoop::add on cfp_object
  //  - use cfp::start on cfp_object to send syn
  try {
    UDPSocket udpsock = UDPSocket::bind("0");
    UDPMux mux{udpsock};

    // read data for first packet
    PayloadT buf;
    file.read(reinterpret_cast<char*>(buf.first.data()), 512);
    buf.second = file.gcount(); // set number of bytes read

    CFP cfp{mux, hostname, port, buf};

    enum {
      OK,
      RETRY_LAST_SEND,
      FILE_EOF
    } sstate = OK;

    while (true) {
      size = udpsock.recvfrom(data, MAXPACKET, &addr);
      try {
        mux.deliver(&addr, data, size);
      } catch (std::out_of_range& e) {
        std::cerr << "received packet not from server" << std::endl;
      }

      // ugly state machine to deal with resending failed sends and file eof
      switch (sstate) {
        case RETRY_LAST_SEND:
          if (!cfp.send(buf)) {
            break; // failed to send, try again next time kiddo
          } // else, fall to next case and send until we can't anymore

        case OK:
          while (!file.eof()) {
            file.read(reinterpret_cast<char*>(buf.first.data()), 512);
            buf.second = file.gcount();

            if (!cfp.send(buf)) {
              sstate = RETRY_LAST_SEND;
              break;
            }
          }

          if (file.eof()) { // hit the end of the file
            cfp.close();
            sstate = FILE_EOF;
          }
          break;

        case FILE_EOF:
          // no more data to transmit, but continue delivering cfp packets
          break;
      }
    }
    return EXIT_SUCCESS;

  } catch (std::runtime_error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
