#include "sockpuppet_test_common.h" // for TestData

#include "sockpuppet/socket_async.h" // for SocketTcpAsync

#include <iostream> // for std::cout
#include <map> // for std::map
#include <thread> // for std::thread

using namespace sockpuppet;

namespace {

size_t const clientCount = 3U;

size_t const testDataSize = 10000000U;
TestData const testData(testDataSize);

std::promise<void> promiseClientsDone;
bool success = true;

struct Server
{
  struct ClientSession
  {
    SocketTcpAsync client;

    ClientSession(Server *parent, SocketTcp &&client)
      : client({std::move(client)},
               parent->driver,
               std::bind(&ClientSession::HandleReceive, this, std::placeholders::_1),
               std::bind(&Server::HandleDisconnect, parent, std::placeholders::_1))
    {
    }
    ClientSession(ClientSession const &) = delete;
    ClientSession(ClientSession &&) = delete;

    void HandleReceive(BufferPtr buffer)
    {
      // echo received data
      (void)client.Send(std::move(buffer));
    }
  };

  AcceptorAsync server;
  Driver &driver;
  std::map<Address, std::unique_ptr<ClientSession>> clientSessions;

  Server(Address bindAddress, Driver &driver)
    : server(MakeTestSocket<Acceptor>(bindAddress),
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
    (void)clientSessions.emplace(
          std::move(clientAddr),
          std::make_unique<ClientSession>(this, std::move(clientSock)));
  }

  void HandleDisconnect(Address clientAddress)
  {
    std::cout << "client "
              << to_string(clientAddress)
              << " closed connection to server" << std::endl;

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
               std::bind(&Clients::HandleDisconnect, parent, std::placeholders::_1))
      , driver(driver)
      , receivedSize(0U)
    {
    }
    Client(Client const &) = delete;
    Client(Client &&) = delete;

    void HandleReceive(BufferPtr buffer)
    {
      receivedSize += buffer->size();

      receivedData.emplace_back(std::move(buffer));

      if(receivedSize == testDataSize) {
        success &= testData.Verify(receivedData);

        // schedule our own disconnect
        // (so we don't destroy our instance from within itself)
        ToDo(
          driver,
          [this]() {
            parent->HandleDisconnect(client.LocalAddress());
          },
          Duration(0));
      }
    }
  };

  std::map<Address, std::unique_ptr<Client>> clients;

  Clients() = default;
  Clients(Clients const &) = delete;
  Clients(Clients &&) = delete;

  void Add(Address serverAddr, Driver &driver)
  {
    auto client = MakeTestSocket<SocketTcp>(serverAddr);
    auto clientAddr = client.LocalAddress();

    std::cout << "client " << to_string(clientAddr)
              << " connected to server" << std::endl;

    (void)clients.emplace(
          std::move(clientAddr),
          std::make_unique<Client>(this, std::move(client), driver));
  }

  void HandleDisconnect(Address clientAddr)
  {
    std::cout << "client "
              << to_string(clientAddr)
              << " closed connection to server" << std::endl;

    clients.erase(clientAddr);
  }
};

void ClientSend(SocketTcpAsync &client)
{
  testData.Send(client);
}

void RunServer(Address serverAddr, Driver &driver)
{
  Server server(serverAddr, driver);

  std::cout << "server listening at " << to_string(serverAddr) << std::endl;

  // run server until stopped by main thread
  driver.Run();
}

void RunClients(Address serverAddr, Driver &driver)
{
  Clients clients;
  for(size_t i = 0U; i < clientCount; ++i) {
    clients.Add(serverAddr, driver);
  }

  // trigger sending from clients to server from multiple threads
  std::thread clientSendThreads[clientCount];
  for(size_t i = 0U; i < clientCount; ++i) {
    auto &&t = clientSendThreads[i];
    auto &&client = std::next(std::begin(clients.clients), i)->second->client;

    t = std::thread(ClientSend, std::ref(client));
  }

  // run clients until stopped by main thread
  driver.Run();

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
  auto futureClientsDone = promiseClientsDone.get_future();

  Driver serverDriver;
  Driver clientDriver;

  Address serverAddr("localhost:8554");

  // set up a server that echoes all input data on multiple sessions
  auto serverThread = std::thread(RunServer, serverAddr, std::ref(serverDriver));

  // set up clients that send to the server and wait for their echo
  // after all data is received back and verified, the connections are closed
  auto clientThread = std::thread(RunClients, serverAddr, std::ref(clientDriver));

  // wait until either the server sessions are closed by the clients or we hit the timeout
  if(futureClientsDone.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
    throw std::runtime_error("clients did not receive echoed reference data on time");
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
