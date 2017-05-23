#include "eventloop.hpp"

#include "cfp.hpp"
#include "udpmux.hpp"
#include "udpsocket.hpp"
#include "util.hpp"

#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

EventLoop::EventLoop(UDPSocket&& udpsock)
    : sock{std::move(udpsock)}, mux{sock }{
  if ((epfd = epoll_create1(0)) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_create1")};
  }

  // register the receiving socket to listen for events
  struct epoll_event ev = {};
  eventinfo.push_back({RECV_EV, nullptr});
  ev.events = EPOLLIN;
  ev.data.ptr = &eventinfo.back();
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock.sockfd, &ev) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }
}

EventLoop::~EventLoop() {
  close(epfd);
}

void EventLoop::run() {
  int ready = 0;

  if ((ready = epoll_wait(epfd, events, MAXEPOLL, -1)) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_wait")};
  }

  for (int i = 0; i < ready; i++) {
    EvInfo* ev_data = static_cast<EvInfo*>(events[i].data.ptr);
    switch (ev_data->type) {
      case RECV_EV: // ev_data->cfp_obj is NULL, don't dereference
        size = sock.recvfrom(data, MAXPACKET, &addr);
        try {
          mux.deliver(&addr, data, size);
        } catch (std::out_of_range& e) {
          throw delivery_exception(&addr, data, size);
        }
        break;

      case RTO_EV:
        static_cast<CFP*>(ev_data->cfp_obj)->timeout_event();
        break;

      case DISCONNECTED_EV:
        static_cast<CFP*>(ev_data->cfp_obj)->disconnect_event();
        break;
    }
  }

}

void EventLoop::add(CFP&& cfp, sockaddr_in* conn_to) {
  protocols.push_back(std::move(cfp));

  CFP& new_cfp = protocols.back();
  mux.connect(&new_cfp, conn_to);

  struct epoll_event ev = {};

  // register retransmission timer with epoll
  eventinfo.push_back({RTO_EV, &new_cfp});
  ev.events = EPOLLIN;
  ev.data.ptr = &eventinfo.back();
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_cfp.rto_timer.timerfd, &ev) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }

  // register connection lost timer with epoll
  eventinfo.push_back({DISCONNECTED_EV, &new_cfp});
  ev.events = EPOLLIN;
  ev.data.ptr = &eventinfo.back();
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_cfp.disconnect_timer.timerfd, &ev) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }
}

const UDPMux& EventLoop::getmux() {
  return mux;
}
