#include "socket.h" // for SocketUdp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

bool success = true;

void Server(SocketAddress serverAddr)
try {
  SocketUdp server(serverAddr);

  std::cout << "receiving at " << std::to_string(serverAddr) << std::endl;

  char buffer[256];
  for(int i = 0; i < 4; ++i) {
    auto received = server.Receive(buffer, sizeof(buffer));
    if(received > 0U &&
       std::string(buffer, received).find("hello") != std::string::npos) {
      auto const t = server.ReceiveFrom(buffer, sizeof(buffer));
      received = std::get<0>(t);
      auto &&clientAddr = std::get<1>(t);

      if(received > 0U &&
         std::string(buffer, received).find("hello") != std::string::npos) {
        std::cout << "received from " << std::to_string(clientAddr) << std::endl;
        return;
      }
    }
  }

  throw std::runtime_error("failed to receive");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddr)
try {
  SocketAddress clientAddr;
  SocketUdp client(clientAddr);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "sending from "
    << std::to_string(clientAddr) << " to "
    << std::to_string(serverAddr) << std::endl;

  for(int i = 0; i < 4; ++i) {
    static char const hello[] = "hello";
    client.SendTo(hello, sizeof(hello), serverAddr);

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
try {
  SocketAddress serverAddr("localhost:8554");

  std::thread server(Server, serverAddr);
  std::thread client(Client, serverAddr);

  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
