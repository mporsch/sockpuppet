#include "sockpuppet/socket_async.h"

#include <chrono> // for std::chrono::steady_clock
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout

using namespace sockpuppet;

static auto startup = std::chrono::steady_clock::now();
static SocketDriver driver;

std::ostream &operator<<(std::ostream &os, std::chrono::steady_clock::time_point tp)
{
  os << (tp - startup).count() / 1000000 << "ms";
  return os;
}

void TellTime(std::chrono::steady_clock::time_point expected)
{
  std::cout << "Now is " << std::chrono::steady_clock::now()
            << "; was scheduled for " << expected
            << std::endl;
}

void RepeatedTellTime(Duration period,
    std::chrono::steady_clock::time_point expected)
{
  TellTime(expected);

  ToDo(driver,
       std::bind(
         RepeatedTellTime,
         period,
         std::chrono::steady_clock::now() + period),
       period);
}

void ShutdownTellTime(std::chrono::steady_clock::time_point expected)
{
  TellTime(expected);
  driver.Stop();
}

int main(int, char **)
try {
  auto now = std::chrono::steady_clock::now();
  ToDo(driver, std::bind(TellTime, now + Duration(50)), Duration(50));
  ToDo(driver, std::bind(RepeatedTellTime, Duration(100), now + Duration(100)), now + Duration(100));

  driver.Step(Duration(150));
  now = std::chrono::steady_clock::now();
  TellTime(now);

  ToDo(driver, std::bind(ShutdownTellTime, now + Duration(2000)), Duration(2000));
  driver.Run();

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
