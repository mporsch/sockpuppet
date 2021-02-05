#include "sockpuppet/socket_async.h"

#include <chrono> // for std::chrono::steady_clock
#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cout

using namespace sockpuppet;

static Driver driver;
static auto startTime = Clock::now();

std::ostream &operator<<(std::ostream &os, TimePoint tp)
{
  using namespace std::chrono;

  auto msecSinceStart = duration_cast<milliseconds>(tp - startTime);
  os << std::setw(4) << msecSinceStart.count() << "ms";
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
  ToDo todo; // ToDo is stored for access with Shift/Cancel
  Duration interval;
  TimePoint next;

  Repeatable(Duration interval, TimePoint until)
    : todo(driver,
           std::bind(&Repeatable::OnTime, this),
           interval) // schedule first execution
    , interval(interval)
    , next(Clock::now() + interval)
  {
    // schedule own ToDo cancellation
    ToDo(driver,
         std::bind(&Repeatable::Cancel, this),
         until);
  }

  void OnTime()
  {
    auto now = next;
    next += interval;

    // reschedule to run again at next interval
    todo.Shift(interval);

    ScheduledPrint("repeating", now);
  }

  void Cancel()
  {
    // everything is running in one thread;
    // there is no danger of race conditions
    // between OnTime and Cancel
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

  // schedule-and-forget task; no need to store the created object
  (void)ToDo(driver,
             std::bind(ScheduledPrint,
                       "once",
                       now + Duration(50)),
             Duration(50));

  // rescheduling task that cancels itself eventually
  Repeatable repeating(Duration(200),
                       now + Duration(1500));

  // task that is scheduled conditionally after
  // being created unscheduled
  ToDo maybe(driver,
             std::bind(ScheduledPrint,
                       "maybe",
                       now + Duration(150)));
  if(true) {
    maybe.Shift(now + Duration(150));
  }

  // schedule task to shut down eventually
  ToDo(driver,
       std::bind(Shutdown,
                 now + Duration(2000)),
       Duration(2000));

  // use the different driver loop methods
  driver.Step(Duration(0));
  driver.Step(Duration(150));
  driver.Step(Duration(-1));
  driver.Run();

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
