#include "socket_async.h" // for SocketUdpAsync
#include "resource_pool.h" // for ResourcePool

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <thread> // for std::this_thread

using namespace sockpuppet;

static std::atomic<bool> success(false);

void HandleReceiveFrom(
  std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress> t)
{
  std::cout << "server received from "
    << std::to_string(std::get<1>(t)) << std::endl;

  success = true;
}

void ReceiveDummy(SocketBuffered::SocketBufferPtr)
{
}

int main(int, char **)
{
  SocketDriver driver;

  SocketAddress serverAddress("localhost:8554");
  SocketUdpAsync server({serverAddress}, driver, HandleReceiveFrom);

  ResourcePool<std::vector<char>> sendPool;
  SocketAddress clientAddress("localhost");
  SocketUdpAsync client({clientAddress}, driver, ReceiveDummy);

  auto thread = std::thread(&SocketDriver::Run, &driver);

  for(int i = 0; i < 5; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto buffer = sendPool.Get(100U);
    client.SendTo(std::move(buffer), serverAddress);
  }

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
