#include "socket.h"

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

bool success = true;

void Server()
try {
  SocketAddress serverAddr("localhost:8554");
  SocketTcpServer server(serverAddr);

  auto t = server.Listen();
  auto &&client = std::get<0>(t);
  auto &&clientAddr = std::get<1>(t);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "transmitting from "
    << std::to_string(serverAddr) << " to "
    << std::to_string(clientAddr) << std::endl;

  static char const hello[] = "hello";
  client.Transmit(hello, sizeof(hello));
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client()
try {
  SocketAddress serverAddr("localhost:8554");
  SocketTcpClient client(serverAddr);

  std::cout << "receiving from "
    << std::to_string(serverAddr) << std::endl;

  char buffer[256];
  for(int i = 0; i < 4; ++i) {
    auto const received = client.Receive(buffer, sizeof(buffer));
    if(received > 0U
    && std::string(buffer, received).find("hello") != std::string::npos) {
      return;
    }
  }

  throw std::runtime_error("failed to receive hello");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  std::thread server(Server);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::thread client(Client);

  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
