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
    : sock{std::move(udpsock)}, mux{sock} {
  if ((epfd = epoll_create1(0)) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_create1")};
  }

  // register the receiving socket to listen for events
  struct epoll_event ev = {};
  eventinfo.push_back({RECV_EV, nullptr});
  ev.events = EPOLLIN;
  ev.data.u32 = eventinfo.size() - 1;

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
    uint32_t ev_idx = events[i].data.u32;
    EvInfo evi = eventinfo[ev_idx];

    switch (evi.type) {
      case RECV_EV: // ev_ptr->cfp_obj is NULL, don't dereference
        std::cerr << "calling recvfrom" << std::endl;
        size = sock.recvfrom(data, MAXPACKET, &addr);
        try {
          mux.deliver(&addr, data, size);
        } catch (std::out_of_range& e) {
          throw delivery_exception(&addr, data, size);
        }
        break;

      case RTO_EV:
        std::cerr << "calling timeout_event" << std::endl;
        evi.cfp_obj->timeout_event();
        break;

      case DISCONNECTED_EV:
        std::cerr << "calling disconnect_event" << std::endl;
        evi.cfp_obj->disconnect_event();
        break;

      default:
        break;
    }
  }
}

CFP& EventLoop::add(CFP&& cfp, sockaddr_in* conn_to) {
  protocols.push_back(std::move(cfp));

  CFP& new_cfp = protocols.back();
  mux.connect(&new_cfp, conn_to);

  struct epoll_event ev = {};

  // register retransmission timer with epoll
  eventinfo.push_back({RTO_EV, &new_cfp});
  ev.events = EPOLLIN;
  ev.data.u32 = eventinfo.size() - 1;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_cfp.rto_timer.timerfd, &ev) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }

  // register connection lost timer with epoll
  eventinfo.push_back({DISCONNECTED_EV, &new_cfp});
  ev.events = EPOLLIN;
  ev.data.u32 = eventinfo.size() - 1;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_cfp.disconnect_timer.timerfd, &ev) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }

  return protocols.back();
}

void EventLoop::remove(CFP& cfp) {
  mux.disconnect(&cfp);

  /* XXX: protocol vector just grows really big
  for (auto i = protocols.begin(); i != protocols.end(); i++) {
    if (cfp.conn_id == i->conn_id) {
      protocols.erase(protocols.begin());
      break;
    }
  }
  */

  if (epoll_ctl(epfd, EPOLL_CTL_DEL, cfp.rto_timer.timerfd, nullptr) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }

  if (epoll_ctl(epfd, EPOLL_CTL_DEL, cfp.disconnect_timer.timerfd, nullptr) == -1) {
    throw std::runtime_error{mkerrorstr("epoll_ctl")};
  }

  /* XXX: eventinfo will just grow forever
  for (auto i = eventinfo.begin(); i != eventinfo.end(); i++) {
    if (i->cfp_obj == &cfp) {
      eventinfo.erase(i);
      break;
    }
  }
  */
}

const UDPMux& EventLoop::getmux() {
  return mux;
}
