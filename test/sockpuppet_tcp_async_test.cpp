#include "socket_async.h" // for SocketTcpAsync
#include "resource_pool.h" // for ResourcePool

#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <map> // for std::map
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <thread> // for std::thread

using namespace sockpuppet;

struct Server
{
  std::map<SocketAddress, SocketTcpAsyncClient> serverHandlers;
  SocketTcpAsyncServer server;
  SocketDriver &driver;
  size_t bytesReceived;
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
      << to_string(clientAddress) << std::endl;

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

    std::cout << "client "
      << to_string(clientAddress)
      << " closed connection to server" << std::endl;

    auto const it = serverHandlers.find(clientAddress);
    if(it != std::end(serverHandlers)) {
      serverHandlers.erase(it);
    }
  }
};

void ReceiveDummy(SocketBuffered::SocketBufferPtr)
{
}

void DisconnectDummy(SocketAddress)
{
}

std::unique_ptr<SocketTcpAsyncClient> leftAloneClient;

void HandleDisconnect(SocketAddress serverAddress)
{
  leftAloneClient.reset();

  std::cout << "server "
    << to_string(serverAddress)
    << " closed connection" << std::endl;
}

static size_t const clientCount = 3U;
static size_t const clientSendCount = 5U;
static size_t const clientSendSize = 1000U;

int main(int, char **)
{
  SocketDriver driver;

  SocketAddress serverAddress("localhost:8554");
  auto server = std::make_unique<Server>(serverAddress, driver);

  auto thread = std::thread(&SocketDriver::Run, &driver);

  // wait for server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  bool success = true;

  {
    ResourcePool<SocketBuffered::SocketBuffer> clientSendPool;
    std::unique_ptr<SocketTcpAsyncClient> clients[clientCount];
    for(auto &&client : clients)
    {
      client.reset(
        new SocketTcpAsyncClient(
          {serverAddress},
          driver,
          ReceiveDummy,
          DisconnectDummy));

      for(size_t i = 0; i < clientSendCount; ++i) {
        auto buffer = clientSendPool.Get(clientSendSize);
        client->Send(std::move(buffer)).wait();
      }
    }

    // wait for everything to be transmitted
    std::this_thread::sleep_for(std::chrono::seconds(1));

    success &= (server->serverHandlers.size() == clientCount);
  }

  // wait for all clients to disconnect
  std::this_thread::sleep_for(std::chrono::seconds(1));

  success &= server->serverHandlers.empty();
  success &= (server->bytesReceived ==
              clientCount
              * clientSendCount
              * clientSendSize);

  // try the disconnect the other way around
  leftAloneClient.reset(
    new SocketTcpAsyncClient(
      {serverAddress},
      driver,
      ReceiveDummy,
      HandleDisconnect));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  server.reset();

  // wait for server handler to disconnect
  std::this_thread::sleep_for(std::chrono::seconds(1));

  success &= !leftAloneClient;

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
