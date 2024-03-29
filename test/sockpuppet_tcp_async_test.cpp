#include "sockpuppet_test_common.h" // for MakeTestSocket

#include "sockpuppet/socket_async.h" // for SocketTcpAsync

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

std::unique_ptr<SocketTcpAsync> loneClient;
std::promise<void> promisedServerDisconnect;

struct Server
{
  AcceptorAsync sock;
  Driver &driver;
  size_t bytesReceived;
  std::map<Address, SocketTcpAsync> serverHandlers;
  std::mutex mtx;

  Server(Address bindAddress,
         Driver &driver)
    : sock(MakeTestSocket<Acceptor>(bindAddress),
           driver,
           std::bind(&Server::HandleConnect,
                     this,
                     std::placeholders::_1,
                     std::placeholders::_2))
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

  void HandleReceive(BufferPtr ptr)
  {
    std::lock_guard<std::mutex> lock(mtx);
    bytesReceived += ptr->size();
  }

  void HandleConnect(SocketTcp clientSock, Address clientAddr)
  {
    std::lock_guard<std::mutex> lock(mtx);

    (void)serverHandlers.emplace(
          std::make_pair(
            std::move(clientAddr),
            SocketTcpAsync({std::move(clientSock), 1U},
                           driver,
                           std::bind(&Server::HandleReceive, this, std::placeholders::_1),
                           std::bind(&Server::HandleDisconnect, this, std::placeholders::_1))));

    if((bytesReceived > 0U) && (serverHandlers.size() == 1U)) {
      promisedLoneClientConnect.set_value();
    } else if(serverHandlers.size() == clientCount) {
      promisedClientsConnect.set_value();
    }
  }

  void HandleDisconnect(Address clientAddress)
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

void ReceiveDummy(BufferPtr)
{
}

void DisconnectDummy(Address, char const *)
{
}

void HandleDisconnect(Address serverAddress, char const *reason)
{
  loneClient.reset();

  std::cout << "server "
            << to_string(serverAddress)
            << " closed connection"
            <<  " (" << reason << ")" << std::endl;

  promisedServerDisconnect.set_value();
}

bool check(char const *message, bool success)
{
  std::cout << message << " - " << (success ? "ok" : "fail") << std::endl;
  return success;
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

  Driver driver;
  auto thread = std::thread(&Driver::Run, &driver);

  auto server = std::make_unique<Server>(Address(), driver);
  auto serverAddr = server->sock.LocalAddress();

  std::cout << "server listening at "
            << to_string(serverAddr)
            << std::endl;

  {
    BufferPool clientSendPool(clientCount * clientSendCount, clientSendSize);
    std::unique_ptr<SocketTcpAsync> clients[clientCount];

    std::vector<std::future<void>> futures;
    futures.reserve(clientCount * clientSendCount);
    for(auto &&client : clients)
    {
      client.reset(new SocketTcpAsync(
          {MakeTestSocket<SocketTcp>(serverAddr)},
          driver,
          ReceiveDummy,
          DisconnectDummy));

      std::cout << "client " << to_string(client->LocalAddress())
                << " connected and sending to server" << std::endl;

      for(size_t i = 0U; i < clientSendCount; ++i) {
        auto buffer = clientSendPool.Get();
        buffer->assign(clientSendSize, 'a');
        futures.push_back(
              client->Send(std::move(buffer)));
      }
    }

    success &= check("wait for all clients to be connected",
        futureClientsConnect.wait_for(seconds(1)) == std::future_status::ready);

    auto deadline = steady_clock::now() + seconds(1);
    for(auto &&future : futures) {
      success &= check("wait for everything to be transmitted",
          future.wait_until(deadline) == std::future_status::ready);
    }

    success &= check("all clients should still be connected before leaving the scope",
        server->ClientCount() == clientCount);
  }

  success &= check("wait for all clients to disconnect",
      futureClientsDisconnect.wait_for(seconds(1)) == std::future_status::ready);

  success &= check("all data should be received",
      server->BytesReceived() ==
          clientCount
          * clientSendCount
          * clientSendSize);

  // try the disconnect the other way around
  loneClient.reset(new SocketTcpAsync(
      {MakeTestSocket<SocketTcp>(serverAddr)},
      driver,
      ReceiveDummy,
      HandleDisconnect));

  success &= check("wait for client to connect",
      futureLoneClientConnect.wait_for(seconds(1)) == std::future_status::ready);

  server.reset();

  success &= check("wait for server handler to disconnect",
      futureServerDisconnect.wait_for(seconds(1)) == std::future_status::ready);

  if(thread.joinable()) {
    driver.Stop();
    thread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
