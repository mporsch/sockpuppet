#include "sockpuppet/socket_async.h"

#include <chrono> // for std::chrono::steady_clock
#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cout

using namespace sockpuppet;

static auto startTime = Clock::now();
static SocketDriver driver;

std::ostream &operator<<(std::ostream &os, TimePoint tp)
{
  os << std::setw(4) << (tp - startTime).count() / 1000000 << "ms";
  return os;
}

void ScheduledPrint(char const *what, TimePoint expected)
{
  std::cout << std::setw(10) << what
            << "; was scheduled for " << expected
            << "; now is " << Clock::now()
            << std::endl;
}

struct Repeatable
{
  ToDo todo;
  Duration period;
  TimePoint next;

  Repeatable(Duration period, TimePoint until)
    : todo(driver, std::bind(&Repeatable::OnTime, this), period)
    , period(period)
    , next(Clock::now() + period)
  {
    ToDo(driver, std::bind(&Repeatable::Cancel, this), until);
  }

  void OnTime()
  {
    auto now = next;
    next += period;

    todo.Shift(period);

    ScheduledPrint("repeating", now);
  }

  void Cancel()
  {
    todo.Cancel();
  }
};

void Shutdown(TimePoint expected)
{
  ScheduledPrint("shutdown", expected);
  driver.Stop();
}

int main(int, char **)
try {
  auto now = Clock::now();

  // schedule-and-forget task
  ToDo(driver, std::bind(ScheduledPrint, "once", now + Duration(50)), Duration(50));

  // rescheduling task that cancels itself eventually
  Repeatable repeating(Duration(200), now + Duration(1500));

  // task that is scheduled conditionally
  ToDo maybe(driver, std::bind(ScheduledPrint, "maybe", now + Duration(150)));
  if(true) {
    maybe.Shift(now + Duration(150));
  }

  driver.Step(Duration(0));
  driver.Step(Duration(150));
  driver.Step(Duration(-1));

  ToDo(driver, std::bind(Shutdown, now + Duration(2000)), Duration(2000));
  driver.Run();

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
