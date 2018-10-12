#include "socket.h"

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

void Server()
{
  SocketAddress src(8554);
  SocketUdp server(src);

  std::cout << "receiving at " << std::to_string(src) << std::endl;

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
  SocketAddress src(0);
  SocketUdp client(src);
  SocketAddress dst("localhost:8554");

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "transmitting from "
    << std::to_string(src) << " to "
    << std::to_string(dst) << std::endl;

  for(int i = 0; i < 4; ++i) {
    static char const hello[] = "hello";
    client.Transmit(hello, sizeof(hello), dst);

    std::this_thread::sleep_for(std::chrono::seconds(1));
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
