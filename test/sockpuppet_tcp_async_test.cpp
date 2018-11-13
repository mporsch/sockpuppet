#include "socket_async.h" // for SocketTcpAsync
#include "resource_pool.h" // for ResourcePool

#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <mutex> // for std::mutex
#include <thread> // for std::thread
#include <vector> // for std::vector

struct Server
{
  SocketTcpAsyncServer server;
  SocketDriver &driver;
  size_t bytesReceived;
  std::deque<SocketTcpAsyncClient> serverHandlers;
  std::mutex mtx;

  Server(SocketAddress bindAddress,
         SocketDriver &driver)
    : server({bindAddress},
             driver,
             std::bind(&Server::HandleConnect, this, std::placeholders::_1))
    , driver(driver)
    , bytesReceived(0U)
  {
  }

  void HandleReceive(SocketBuffered::SocketBufferPtr ptr)
  {
    std::lock_guard<std::mutex> lock(mtx);

    bytesReceived += ptr->size();
  }

  void HandleConnect(std::tuple<SocketTcpClient, SocketAddress> t)
  {
    std::lock_guard<std::mutex> lock(mtx);

    std::cout << "server accepted connection from "
      << std::to_string(std::get<1>(t)) << std::endl;

    serverHandlers.emplace_back(
        SocketTcpAsyncClient({std::move(std::get<0>(t))},
                             driver,
                             std::bind(&Server::HandleReceive, this, std::placeholders::_1),
                             std::bind(&Server::HandleDisconnect, this, std::placeholders::_1)));
  }

  void HandleDisconnect(SocketTcpAsyncClient *instance)
  {
    std::lock_guard<std::mutex> lock(mtx);

    serverHandlers.erase(
          std::remove_if(std::begin(serverHandlers),
                         std::end(serverHandlers),
                         [&](SocketTcpAsyncClient const &client) -> bool
                         {
                           return (&client == instance);
                         }),
          std::end(serverHandlers));
  }
};

int main(int, char **)
{
  SocketDriver driver;

  SocketAddress serverAddress(8554);
  Server server(serverAddress, driver);

  auto thread = std::thread(&SocketDriver::Run, &driver);

  ResourcePool<SocketBuffered::SocketBuffer> clientSendPool;
  SocketTcpAsyncClient clients[] = {
    SocketTcpAsyncClient({serverAddress}, driver)
  , SocketTcpAsyncClient({serverAddress}, driver)
  , SocketTcpAsyncClient({serverAddress}, driver)
  };
  for(auto &&client : clients)
  {
    for(int i = 0; i < 3; ++i) {
      auto buffer = clientSendPool.Get(100U);
      client.Send(std::move(buffer)).wait();
    }
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (server.bytesReceived == 900U ? EXIT_SUCCESS : EXIT_FAILURE);
}
