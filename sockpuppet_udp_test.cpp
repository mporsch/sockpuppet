#include "socket.h"

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

void Server()
{
  SocketUdp server(SocketAddress("localhost:8554"));

  char buffer[256];
  for(int i = 0; i < 4; ++i) {
    int const received = server.Receive(buffer, sizeof(buffer));
    if(received > 0U
    && std::string(buffer, received).find("hello") != std::string::npos) {
      return;
    }
  }

  throw std::runtime_error("failed to receive hello");
}

void Client()
{
  SocketUdp client(SocketAddress(0));

  for(int i = 0; i < 4; ++i) {
    static char const hello[] = "hello";

    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.Transmit(hello, sizeof(hello), SocketAddress("localhost:8554"));
  }
}

int main(int, char **)
try {
  std::thread server(Server);
  std::thread client(Client);

  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
