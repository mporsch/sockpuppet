#include "sockpuppet/socket.h" // for SocketTcpClient

#include <atomic> // for std::atomic
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string
#include <thread> // for std::thread

using namespace sockpuppet;
using namespace std::chrono;

static int const clientCount = 3;

static std::atomic<bool> success(true);

void ServerHandler(std::pair<SocketTcpClient, Address> p)
try {
  auto &&clientSock = p.first;
  auto &&clientAddr = p.second;

  char buffer[256];
  if(clientSock.Receive(buffer, sizeof(buffer), seconds(0)) != 0U) {
    throw std::runtime_error("unexpected receive");
  }

  std::cout << "server sending to client " << to_string(clientAddr) << std::endl;

  static char const hello[] = "hello";
  (void)clientSock.Send(hello, sizeof(hello));

  // destroying the handler socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Server(Address serverAddr)
try {
  SocketTcpServer server(serverAddr);

  std::cout << "server listening at " << to_string(serverAddr)
    << std::endl;

  std::thread serverHandlers[clientCount];
  for(auto &&serverHandler : serverHandlers) {
    serverHandler = std::thread(
          ServerHandler,
          server.Listen(seconds(2)));
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
  SocketTcpClient client(serverAddr);
  auto const clientAddr = client.LocalAddress();

  std::cout << "client " << to_string(clientAddr)
    << " connected to server " << to_string(serverAddr)
    << std::endl;

  char buffer[256];
  auto const received =
    client.Receive(buffer, sizeof(buffer), seconds(1));
  if(received > 0U &&
     std::string(buffer, received).find("hello") != std::string::npos) {
    std::cout << "client " << to_string(clientAddr)
      << " received from server" << std::endl;

    try {
      // the server closes the connection after the "hello" message
      // we expect to get the corresponding exception now
      (void)client.Receive(buffer, sizeof(buffer), seconds(1));
      success = false;
    } catch(std::exception const &e) {
      std::cout << e.what() << std::endl;
      return;
    }
  }

  throw std::runtime_error("client failed to receive");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  Address const serverAddr("localhost:8554");
  std::thread server(Server, serverAddr);

  // wait for server thread to come up
  std::this_thread::sleep_for(seconds(1));

  std::thread clients[clientCount];
  for(auto &&client : clients) {
    client = std::thread (Client, serverAddr);
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
