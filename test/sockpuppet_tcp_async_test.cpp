#include "socket_async.h" // for SocketTcpAsync
#include "resource_pool.h" // for ResourcePool

#include <iostream> // for std::cout
#include <map> // for std::map
#include <mutex> // for std::mutex
#include <thread> // for std::thread

using namespace sockpuppet;

namespace {

size_t const clientCount = 3U;
size_t const clientSendCount = 5U;
size_t const clientSendSize = 1000U;

std::promise<void> promisedClientsConnect;
std::promise<void> promisedClientsDisconnect;
std::promise<void> promisedLoneClientConnect;

std::unique_ptr<SocketTcpAsyncClient> loneClient;
std::promise<void> promisedServerDisconnect;

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

  size_t ClientCount()
  {
    std::lock_guard<std::mutex> lock(mtx);
    return serverHandlers.size();
  }

  size_t BytesReceived()
  {
    std::lock_guard<std::mutex> lock(mtx);
    return bytesReceived;
  }

  void HandleReceive(SocketBuffered::SocketBufferPtr ptr)
  {
    std::lock_guard<std::mutex> lock(mtx);
    bytesReceived += ptr->size();
  }

  void HandleConnect(std::tuple<SocketTcpClient, SocketAddress> t)
  {
    auto &&clientAddress = std::get<1>(t);

    std::lock_guard<std::mutex> lock(mtx);

    (void)serverHandlers.emplace(
          std::make_pair(
            clientAddress,
            SocketTcpAsyncClient({std::move(std::get<0>(t))},
                                 driver,
                                 std::bind(&Server::HandleReceive, this, std::placeholders::_1),
                                 std::bind(&Server::HandleDisconnect, this, std::placeholders::_1))));

    if(serverHandlers.size() == clientCount) {
      promisedClientsConnect.set_value();
    } else if ((bytesReceived > 0U) &&
               (serverHandlers.size() == 1U)) {
      promisedLoneClientConnect.set_value();
    }
  }

  void HandleDisconnect(SocketAddress clientAddress)
  {
    std::cout << "client "
              << to_string(clientAddress)
              << " closed connection to server" << std::endl;

    std::lock_guard<std::mutex> lock(mtx);

    serverHandlers.erase(clientAddress);

    if(serverHandlers.empty()) {
      promisedClientsDisconnect.set_value();
    }
  }
};

void ReceiveDummy(SocketBuffered::SocketBufferPtr)
{
}

void DisconnectDummy(SocketAddress)
{
}

void HandleDisconnect(SocketAddress serverAddress)
{
  loneClient.reset();

  std::cout << "server "
            << to_string(serverAddress)
            << " closed connection" << std::endl;

  promisedServerDisconnect.set_value();
}

} // unnamed namespace

int main(int, char **)
{
  using namespace std::chrono;

  bool success = true;

  // futures to check / wait for asynchronous events
  auto futureClientsConnect = promisedClientsConnect.get_future();
  auto futureClientsDisconnect = promisedClientsDisconnect.get_future();
  auto futureLoneClientConnect = promisedLoneClientConnect.get_future();
  auto futureServerDisconnect = promisedServerDisconnect.get_future();

  SocketDriver driver;
  auto thread = std::thread(&SocketDriver::Run, &driver);

  SocketAddress serverAddress("localhost:8554");
  auto server = std::make_unique<Server>(serverAddress, driver);

  std::cout << "server listening at " << to_string(serverAddress)
            << std::endl;

  {
    SocketAsync::SocketBufferPool clientSendPool;
    std::unique_ptr<SocketTcpAsyncClient> clients[clientCount];

    std::vector<std::future<void>> futures;
    futures.reserve(clientCount * clientSendCount);
    for(auto &&client : clients)
    {
      client.reset(
            new SocketTcpAsyncClient({serverAddress},
                                     driver,
                                     ReceiveDummy,
                                     DisconnectDummy));

      std::cout << "client " << to_string(client->LocalAddress())
                << " connected and sending to server" << std::endl;

      for(size_t i = 0U; i < clientSendCount; ++i) {
        auto buffer = clientSendPool.Get(clientSendSize);
        futures.push_back(
              client->Send(std::move(buffer)));
      }
    }

    // wait for all clients to be connected
    success &= (futureClientsConnect.wait_for(seconds(1)) == std::future_status::ready);

    // wait for everything to be transmitted
    auto deadline = steady_clock::now() + seconds(1);
    for(auto &&future : futures) {
      success &= (future.wait_until(deadline) == std::future_status::ready);
    }

    // all clients should still be connected before leaving the scope
    success &= (server->ClientCount() == clientCount);
  }

  // wait for all clients to disconnect
  success &= (futureClientsDisconnect.wait_for(seconds(1)) == std::future_status::ready);

  // all data should be received
  success &= (server->BytesReceived() ==
              clientCount
              * clientSendCount
              * clientSendSize);

  // try the disconnect the other way around
  loneClient.reset(
        new SocketTcpAsyncClient({serverAddress},
                                 driver,
                                 ReceiveDummy,
                                 HandleDisconnect));

  // wait for client to connect
  success &= (futureLoneClientConnect.wait_for(seconds(1)) == std::future_status::ready);

  server.reset();

  // wait for server handler to disconnect
  success &= (futureServerDisconnect.wait_for(seconds(1)) == std::future_status::ready);

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
