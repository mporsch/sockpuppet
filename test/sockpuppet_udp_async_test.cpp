#include "sockpuppet/socket_async.h" // for SocketUdpAsync

#include <iostream> // for std::cout
#include <thread> // for std::this_thread

using namespace sockpuppet;

static auto promisedReceipt = std::make_unique<std::promise<void>>();
static size_t const clientSendCount = 5U;
static size_t const clientSendSize = 100U;

void HandleReceiveFrom(BufferPtr, Address addr)
{
  std::cout << "received from "
            << to_string(addr) << std::endl;

  if(promisedReceipt) {
    promisedReceipt->set_value();
    promisedReceipt.reset();
  }
}

void ReceiveFromDummy(BufferPtr, Address)
{
}

int main(int, char **)
{
  using namespace std::chrono;

  bool success = true;

  Driver driver;
  auto thread = std::thread(&Driver::Run, &driver);

  {
    auto serverSock = SocketUdpAsync(
        {Address(), 1U, 1500U},
        driver,
        HandleReceiveFrom);
    auto serverAddr = serverSock.LocalAddress();

    std::cout << "waiting for receipt at "
              << to_string(serverAddr)
              << std::endl;

    auto futureReceipt = promisedReceipt->get_future();

    {
      BufferPool sendPool(clientSendCount, clientSendSize);

      auto clientSock = SocketUdpAsync(
          {Address()},
          driver,
          ReceiveFromDummy);

      std::cout << "sending from " << to_string(clientSock.LocalAddress())
                << " to " << to_string(serverAddr) << std::endl;

      std::vector<std::future<void>> futuresSend;
      futuresSend.reserve(clientSendCount);
      for(size_t i = 0U; i < clientSendCount; ++i) {
        auto buffer = sendPool.Get();
        buffer->assign(clientSendSize, 'a');
        futuresSend.emplace_back(clientSock.SendTo(std::move(buffer), serverAddr));
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
