#ifndef _EVENTLOOP_HPP
#define _EVENTLOOP_HPP

#include "udpmux.hpp"
#include "udpsocket.hpp"

#include <sys/epoll.h>
#include <vector>

// max number of clients * 2 + 1
#define MAXEPOLL 21

enum EvType {
  RECV_EV,
  RTO_EV,
  DISCONNECTED_EV
};

struct EvInfo {
  EvType type;
  CFP* cfp_obj;
};

class UDPMux;
class CFP;

class delivery_exception : std::exception {
 public:
  delivery_exception(sockaddr_in* addr, uint8_t* data, size_t size)
      : addr{addr}, data{data}, size{size} {}
  sockaddr_in* addr;
  uint8_t* data;
  size_t size;
};

class EventLoop {
 public:
  explicit EventLoop(UDPSocket&& udpsock);
  ~EventLoop();
  void run();
  void add(CFP&& cfp, sockaddr_in* conn_to);
  const UDPMux& getmux();

 private:
  UDPSocket sock;
  UDPMux mux;
  std::vector<CFP> protocols;

  uint8_t data[MAXPACKET];
  size_t size;
  sockaddr_in addr;

  int epfd;
  epoll_event events[MAXEPOLL];
  std::vector<EvInfo> eventinfo;
};

#endif // _EVENTLOOP_HPP
