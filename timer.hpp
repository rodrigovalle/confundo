#ifndef _TIMER_HPP
#define _TIMER_HPP

class EventLoop;
class Timer {
 friend EventLoop;
 public:
  Timer();
  Timer(Timer&& other) noexcept;
  ~Timer();

  /* sets a new timeout, cancelling the old one */
  void set_timeout(double timeout); // arm the timer
  void read();
  void cancel_timeout(); // disarm the timer

 private:
  int timerfd;
  double timeout;
};

#endif // _TIMER_HPP
