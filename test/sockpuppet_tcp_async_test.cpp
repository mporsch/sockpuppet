#include "socket_async.h" // for SocketTcpAsync
#include "resource_pool.h" // for ResourcePool

#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <map> // for std::map
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <thread> // for std::thread

struct Server
{
  SocketTcpAsyncServer server;
  SocketDriver &driver;
  size_t bytesReceived;
  std::map<SocketAddress, SocketTcpAsyncClient> serverHandlers;
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

    auto &&clientAddress = std::get<1>(t);

    std::cout << "server accepted connection from "
      << std::to_string(clientAddress) << std::endl;

    (void)serverHandlers.emplace(
      std::make_pair(
        clientAddress,
        SocketTcpAsyncClient({std::move(std::get<0>(t))},
                             driver,
                             std::bind(&Server::HandleReceive, this, std::placeholders::_1),
                             std::bind(&Server::HandleDisconnect, this, std::placeholders::_1))));
  }

  void HandleDisconnect(SocketAddress clientAddress)
  {
    std::lock_guard<std::mutex> lock(mtx);

    std::cout << "server closing connection to "
      << std::to_string(clientAddress) << std::endl;

    auto const it = serverHandlers.find(clientAddress);
    if(it != std::end(serverHandlers)) {
      serverHandlers.erase(it);
    }
  }
};

static size_t const clientCount = 3U;
static size_t const clientSendCount = 5U;
static size_t const clientSendSize = 1000U;

int main(int, char **)
{
  SocketDriver driver;

  SocketAddress serverAddress("localhost:8554");
  Server server(serverAddress, driver);

  auto thread = std::thread(&SocketDriver::Run, &driver);

  // wait for server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  bool success = true;

  {
    ResourcePool<SocketBuffered::SocketBuffer> clientSendPool;
    std::unique_ptr<SocketTcpAsyncClient> clients[clientCount];
    for(auto &&client : clients)
    {
      client.reset(new SocketTcpAsyncClient({serverAddress}, driver));

      for(size_t i = 0; i < clientSendCount; ++i) {
        auto buffer = clientSendPool.Get(clientSendSize);
        client->Send(std::move(buffer)).wait();
      }
    }

    success &= (server.serverHandlers.size() == clientCount);

    // wait for everything to be transmitted
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // wait for all clients to disconnect
  std::this_thread::sleep_for(std::chrono::seconds(1));

  success &= server.serverHandlers.empty();

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  success &= (server.bytesReceived == clientCount * clientSendCount * clientSendSize);

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
