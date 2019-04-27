#include "socket.h" // for SocketTcpClient

#ifdef HAVE_HELPER
# include "print_unmangled.h" // for PrintUnmangled

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

using namespace sockpuppet;

static int const clientCount = 3;

bool success = true;

void ServerHandler(std::tuple<SocketTcpClient, SocketAddress> t)
try {
  auto &&handler = std::get<0>(t);
  auto &&clientAddr = std::get<1>(t);

  COUT << "server sending to "
    << std::to_string(clientAddr) << std::endl;

  static char const hello[] = "hello";
  handler.Send(hello, sizeof(hello));

  // destroying the handler socket closes the connection
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
    serverHandler = std::thread(
          ServerHandler,
          server.Listen(std::chrono::seconds(2)));
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
  auto const received = client.Receive(buffer, sizeof(buffer),
                                       std::chrono::seconds(1));
  if(received > 0U &&
     std::string(buffer, received).find("hello") != std::string::npos) {
    COUT << "client received from "
      << std::to_string(serverAddr) << std::endl;

    try {
      // the server closes the connection after the "hello" message
      // we expect to get the corresponding exception now
      (void)client.Receive(buffer, sizeof(buffer),
                           std::chrono::seconds(1));
      success = false;
    } catch(std::exception const &e) {
      COUT << e.what() << std::endl;
      return;
    }
  }

  throw std::runtime_error("client failed to receive");
} catch (std::exception const &e) {
  CERR << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  SocketAddress const serverAddr("localhost:8554");
  std::thread server(Server, serverAddr);

  // wait for server thread to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));

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
