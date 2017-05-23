class Timer {
 public:
  Timer();
  Timer(Timer&& other) noexcept;
  ~Timer();

  /* sets a new timeout, cancelling the old one */
  void set_timeout(double timeout); // arm the timer
  void cancel_timeout(); // disarm the timer

 private:
  int timerfd;
  double timeout;
};
