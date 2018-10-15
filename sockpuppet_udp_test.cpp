#include "socket.h"

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

bool success = true;

void Server()
try {
  SocketAddress src(8554);
  SocketUdp server(src);

  std::cout << "receiving at " << std::to_string(src) << std::endl;

  char buffer[256];
  for(int i = 0; i < 4; ++i) {
    auto received = server.Receive(buffer, sizeof(buffer));
    if(received > 0U
    && std::string(buffer, received).find("hello") != std::string::npos) {
      auto const t = server.ReceiveFrom(buffer, sizeof(buffer));
      received = std::get<0>(t);
      auto &&sender = std::get<1>(t);

      if(received > 0U
      && std::string(buffer, received).find("hello") != std::string::npos) {
        std::cout << "successfully received from " << std::to_string(sender) << std::endl;
        return;
      }
    }
  }

  throw std::runtime_error("failed to receive hello");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client()
try {
  SocketAddress src(0);
  SocketUdp client(src);
  SocketAddress dst("localhost:8554");

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "sending from "
    << std::to_string(src) << " to "
    << std::to_string(dst) << std::endl;

  for(int i = 0; i < 4; ++i) {
    static char const hello[] = "hello";
    client.SendTo(hello, sizeof(hello), dst);

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  std::thread server(Server);
  std::thread client(Client);

  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
