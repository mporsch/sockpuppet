#include "socket.h"

#ifdef HAVE_HELPER
# include "../helper/print_unmangled.h"

# define COUT PrintUnmangled()
# define CERR PrintUnmangled(std::cerr)
#else
# define COUT std::cout
# define CERR std::cerr
#endif // HAVE_HELPER

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

static int const clientCount = 3;
bool success = true;

void ServerHandler(SocketTcpClient client, SocketAddress clientAddr)
try {
  std::this_thread::sleep_for(std::chrono::seconds(1));

  COUT << "server transmitting to "
    << std::to_string(clientAddr) << std::endl;

  static char const hello[] = "hello";
  client.Transmit(hello, sizeof(hello));
} catch (std::exception const &e) {
  CERR << e.what() << std::endl;
  success = false;
}

void Server(SocketAddress serverAddr)
try {
  SocketTcpServer server(serverAddr);

  COUT << "server listening at "
    << std::to_string(serverAddr) << std::endl;

  std::thread serverHandlers[clientCount];
  for(auto &&serverHandler : serverHandlers) {
    auto t = server.Listen();
    auto &&client = std::get<0>(t);
    auto &&clientAddr = std::get<1>(t);

    serverHandler = std::thread(ServerHandler,
                                std::move(client),
                                std::move(clientAddr));
  }

  for(auto &&serverHandler : serverHandlers) {
    if(serverHandler.joinable()) {
      serverHandler.join();
    }
  }
} catch (std::exception const &e) {
  CERR << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddr)
try {
  SocketTcpClient client(serverAddr);

  COUT << "client " << std::this_thread::get_id()
    << " connected to " << std::to_string(serverAddr)
    << std::endl;

  char buffer[256];
  auto const received = client.Receive(buffer, sizeof(buffer));
  if(received > 0U
  && std::string(buffer, received).find("hello") != std::string::npos) {
    return;
  }

  throw std::runtime_error("failed to receive hello");
} catch (std::exception const &e) {
  CERR << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  std::thread server(Server, SocketAddress("localhost:8554"));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::thread clients[clientCount];
  for(auto &&client : clients) {
    client = std::thread (Client, SocketAddress("localhost:8554"));
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
