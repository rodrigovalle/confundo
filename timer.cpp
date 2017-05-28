#include "timer.hpp"
#include "util.hpp"

#include <cmath>
#include <sys/timerfd.h>
#include <unistd.h>

Timer::Timer() {
  if ((timerfd = ::timerfd_create(CLOCK_MONOTONIC, 0)) == -1) {
    throw std::runtime_error{mkerrorstr("timerfd_create")};
  }
}

Timer::Timer(Timer&& other) noexcept {
  timerfd = other.timerfd;
  timeout = other.timeout;

  other.timerfd = -1;
}

Timer::~Timer() {
  close(timerfd);
}

void Timer::set_timeout(double timeout) {
  struct itimerspec timer = {};
  timer.it_value.tv_sec = static_cast<int>(floor(timeout));
  timer.it_value.tv_nsec = (timeout - floor(timeout)) * 1000000000;

  if (::timerfd_settime(timerfd, 0, &timer, nullptr) == -1) {
    throw std::runtime_error{mkerrorstr("timerfd_settime")};
  }
}

/* reads the data from timerfd so we can continue polling
 * call only after a timeout event otherwise it will block */
void Timer::read() {
  uint8_t buf;
  ::read(timerfd, &buf, 8);
}

void Timer::cancel_timeout() {
  struct itimerspec timer = {};

  if (::timerfd_settime(timerfd, 0, &timer, nullptr) == -1) {
    throw std::runtime_error{mkerrorstr("timerfd_settime")};
  }
}
