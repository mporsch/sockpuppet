#include "sockpuppet_test_common.h" // for MakeTestSocket

#include "sockpuppet/socket.h" // for SocketTcp

#include <atomic> // for std::atomic
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <stdexcept> // for std::runtime_error
#include <string_view> // for std::string_view
#include <thread> // for std::thread

using namespace sockpuppet;
using namespace std::chrono;

static int const clientCount = 3;

static std::atomic<bool> success(true);

void ServerHandler(std::pair<SocketTcp, Address> p)
try {
  auto &&clientSock = p.first;
  auto &&clientAddr = p.second;

  std::cout << "server sending to client " << to_string(clientAddr) << std::endl;

  static char const hello[] = "hello";
  (void)clientSock.Send(hello, sizeof(hello));

  // destroying the handler socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Server(Acceptor serverSock)
try {
  std::cout << "server listening at "
            << to_string(serverSock.LocalAddress())
            << std::endl;

  std::thread serverHandlers[clientCount];
  for(auto &&serverHandler : serverHandlers) {
    serverHandler = std::thread(
          ServerHandler,
          serverSock.Listen(seconds(2)).value());
  }

  for(auto &&serverHandler : serverHandlers) {
    if(serverHandler.joinable()) {
      serverHandler.join();
    }
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddr)
try {
  auto clientSock = MakeTestSocket<SocketTcp>(serverAddr);
  auto clientAddr = clientSock.LocalAddress();

  std::cout << "client " << to_string(clientAddr)
            << " connected to server " << to_string(serverAddr)
            << std::endl;

  char buffer[256];
  constexpr Duration receiveTimeout = seconds(1);
  if(auto rx = clientSock.Receive(buffer, sizeof(buffer), receiveTimeout)) {
    if(std::string_view(buffer, rx.value()).find("hello") != std::string::npos) {
      std::cout << "client " << to_string(clientAddr)
                << " received from server" << std::endl;

      try {
        // the server closes the connection after the "hello" message
        // we expect to get the corresponding exception now
        (void)clientSock.Receive(buffer, sizeof(buffer), receiveTimeout);
        success = false;
      } catch(std::exception const &e) {
        std::cout << e.what() << std::endl;
        return;
      }
    }
  }

  throw std::runtime_error("client failed to receive");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  auto serverSock = MakeTestSocket<Acceptor>(Address());
  auto serverAddr = serverSock.LocalAddress();

  std::thread server(Server, std::move(serverSock));

  // wait for server thread to come up
  std::this_thread::sleep_for(seconds(1));

  std::thread clients[clientCount];
  for(auto &&client : clients) {
    client = std::thread(Client, serverAddr);
  }

  if(server.joinable()) {
    server.join();
  }
  for(auto &&client : clients) {
    if(client.joinable()) {
      client.join();
    }
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
