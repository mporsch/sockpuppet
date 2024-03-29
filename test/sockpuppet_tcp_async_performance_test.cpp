#include "sockpuppet_test_common.h" // for TestData

#include "sockpuppet/socket_async.h" // for SocketTcpAsync

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <map> // for std::map
#include <thread> // for std::thread

using namespace sockpuppet;

namespace {

size_t const clientCount = 3U;

constexpr size_t testDataSize = 10U * 1024 * 1024;
TestData const testData(testDataSize);

std::promise<void> promiseClientsDone;
std::atomic<bool> success = true;

struct Server
{
  struct ClientSession
  {
    SocketTcpAsync clientSock;
    unsigned int receiveCount = 0U;

    ClientSession(Server *parent, SocketTcp &&clientSock)
      : clientSock({std::move(clientSock)},
                   parent->driver,
                   std::bind(&ClientSession::HandleReceive, this, std::placeholders::_1),
                   std::bind(&Server::HandleDisconnect, parent, std::placeholders::_1, std::placeholders::_2))
    {
    }
    ClientSession(ClientSession const &) = delete;
    ClientSession(ClientSession &&) = delete;

    void HandleReceive(BufferPtr buffer)
    {
      // simulate some processing delay to trigger TCP congestion control
      if(++receiveCount % 1000 == 0U) { // sleeping usec intervals is inaccurate
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      // echo received data
      (void)clientSock.Send(std::move(buffer));
    }
  };

  AcceptorAsync serverSock;
  Driver &driver;
  std::map<Address, std::unique_ptr<ClientSession>> clientSessions;

  Server(Acceptor serverSock, Driver &driver)
    : serverSock(std::move(serverSock),
                 driver,
                 std::bind(&Server::HandleConnect,
                           this,
                           std::placeholders::_1,
                           std::placeholders::_2))
    , driver(driver)
  {
  }
  Server(Server const &) = delete;
  Server(Server &&) = delete;

  void HandleConnect(SocketTcp clientSock, Address clientAddr)
  {
    std::cout << "client " << to_string(clientAddr)
              << " connected to server" << std::endl;

    (void)clientSessions.emplace(
          std::move(clientAddr),
          std::make_unique<ClientSession>(this, std::move(clientSock)));
  }

  void HandleDisconnect(Address clientAddress, char const *reason)
  {
    std::cout << "client " << to_string(clientAddress)
              << " closed connection to server"
              << " (" << reason << ")" << std::endl;

    clientSessions.erase(clientAddress);

    // fulfill done promise after all clients have received, verified and disconnected
    if(clientSessions.empty()) {
      promiseClientsDone.set_value();
    }
  }
};

struct Clients
{
  struct Client
  {
    Clients *parent;
    SocketTcpAsync client;
    Driver &driver;
    size_t receivedSize;
    std::vector<BufferPtr> receivedData;

    Client(Clients *parent, SocketTcp client, Driver &driver)
      : parent(parent)
      , client({std::move(client)},
               driver,
               std::bind(&Client::HandleReceive, this, std::placeholders::_1),
               std::bind(&Clients::HandleDisconnect, parent, std::placeholders::_1, std::placeholders::_2))
      , driver(driver)
      , receivedSize(0U)
    {
      receivedData.reserve(testDataSize / TestData::tcpPacketSizeMin);
    }
    Client(Client const &) = delete;
    Client(Client &&) = delete;

    void HandleReceive(BufferPtr buffer)
    {
      receivedSize += buffer->size();

      receivedData.emplace_back(std::move(buffer));

      if(receivedSize == testDataSize) {
        if(!testData.Verify(receivedData)) {
          success = false;
        }

        // schedule our own disconnect
        // (so we don't destroy our instance from within itself)
        ToDo(
          driver,
          [this]() {
            parent->HandleDisconnect(client.LocalAddress(), "self-initiated shutdown");
          },
          Duration(0));
      }
    }
  };

  std::map<Address, std::unique_ptr<Client>> clients;

  Clients() = default;
  Clients(Clients const &) = delete;
  Clients(Clients &&) = delete;

  SocketTcpAsync &Add(Address serverAddr, Driver &driver)
  {
    auto clientSock = MakeTestSocket<SocketTcp>(serverAddr);
    auto clientAddr = clientSock.LocalAddress();

    std::cout << "client " << to_string(clientAddr)
              << " connecting to server" << std::endl;

    auto p = clients.emplace(
          std::move(clientAddr),
          std::make_unique<Client>(this, std::move(clientSock), driver));
    return p.first->second->client;
  }

  void HandleDisconnect(Address clientAddr, char const *reason)
  {
    std::cout << "client " << to_string(clientAddr)
              << " closing connection to server"
              << " (" << reason << ")" << std::endl;

    clients.erase(clientAddr);
  }
};

void ClientSend(SocketTcpAsync &client)
{
  try {
    testData.Send(client);
  } catch(std::exception const &e) {
    std::cerr << e.what() << std::endl;
    success = false;
  }
}

void RunServer(Acceptor serverSock, Driver &driver)
{
  std::cout << "server listening at "
            << to_string(serverSock.LocalAddress())
            << std::endl;

  auto server = Server(std::move(serverSock), driver);

  // run server until stopped by main thread
  driver.Run();
}

void RunClients(Address serverAddr, Driver &driver)
{
  std::thread clientSendThreads[clientCount];

  {
    Clients clients;

    // create multiple client connections and
    // trigger sending to server from multiple threads
    for(size_t i = 0U; i < clientCount; ++i) {
      auto &client = clients.Add(serverAddr, driver);
      clientSendThreads[i] = std::thread(ClientSend, std::ref(client));
    }

    // run clients until stopped by main thread
    driver.Run();
  } // release clients to break pending send promises

  // wait for the sending threads to finish
  for(auto &&t : clientSendThreads) {
    if(t.joinable()) {
      t.join();
    }
  }
}

} // unnamed namespace

int main(int, char **)
try {
  using namespace std::chrono_literals;

  auto futureClientsDone = promiseClientsDone.get_future();

  Driver serverDriver;
  Driver clientDriver;

  auto serverSock = MakeTestSocket<Acceptor>(Address());
  auto serverAddr = serverSock.LocalAddress();

  // set up a server that echoes all input data on multiple sessions
  auto serverThread = std::thread(RunServer, std::move(serverSock), std::ref(serverDriver));

  // wait for server to come up
  std::this_thread::sleep_for(1s);

  // set up clients that send to the server and wait for their echo
  // after all data is received back and verified, the connections are closed
  auto clientThread = std::thread(RunClients, serverAddr, std::ref(clientDriver));

  // wait until either the server sessions are closed by the clients or we hit the timeout
  if(futureClientsDone.wait_for(60s) != std::future_status::ready) {
    std::cerr << "clients did not receive echoed reference data on time"
              << std::endl;
    success = false;
  }

  // stop the drivers after the sockets have been shut down to allow proper TLS shutdown
  clientDriver.Stop();
  if(clientThread.joinable()) {
    clientThread.join();
  }
  serverDriver.Stop();
  if(serverThread.joinable()) {
    serverThread.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
