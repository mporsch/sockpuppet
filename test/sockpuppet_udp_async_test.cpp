#include "socket_async.h" // for SocketUdpAsync
#include "resource_pool.h" // for ResourcePool

#include <iostream> // for std::cout
#include <thread> // for std::this_thread

using namespace sockpuppet;

static auto promisedReceipt = std::make_unique<std::promise<void>>();

void HandleReceiveFrom(
    std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress> t)
{
  std::cout << "received from "
            << to_string(std::get<1>(t)) << std::endl;

  if(promisedReceipt) {
    promisedReceipt->set_value();
    promisedReceipt.reset();
  }
}

void ReceiveDummy(SocketBuffered::SocketBufferPtr)
{
}

int main(int, char **)
{
  using namespace std::chrono;

  const int sendCount = 5;

  bool success = true;

  SocketDriver driver;
  auto thread = std::thread(&SocketDriver::Run, &driver);

  {
    SocketAddress serverAddress("localhost:8554");
    SocketUdpAsync server({serverAddress}, driver, HandleReceiveFrom);

    std::cout << "waiting for receipt at " << to_string(serverAddress)
              << std::endl;

    auto futureReceipt = promisedReceipt->get_future();

    {
      ResourcePool<std::vector<char>> sendPool;

      SocketUdpAsync client({SocketAddress("localhost")},
                            driver,
                            ReceiveDummy);

      std::cout << "sending from " << to_string(client.LocalAddress())
                << " to " << to_string(serverAddress) << std::endl;

      std::vector<std::future<void>> futuresSend;
      futuresSend.reserve(sendCount);
      for(int i = 0; i < sendCount; ++i) {
        auto buffer = sendPool.Get(100U);
        futuresSend.emplace_back(
              client.SendTo(std::move(buffer), serverAddress));
      }

      auto deadline = steady_clock::now() + seconds(1);
      for(auto &&future : futuresSend) {
        success &= (future.wait_until(deadline) == std::future_status::ready);
      }
    }

    success &= (futureReceipt.wait_for(seconds(1)) == std::future_status::ready);
  }

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
