#include "sockpuppet/socket.h" // for SocketUdp

#include <atomic> // for std::atomic
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <stdexcept> // for std::runtime_error
#include <string_view> // for std::string_view
#include <thread> // for std::thread

using namespace sockpuppet;

static std::atomic<bool> success(true);

void Server(Address serverAddr)
try {
  SocketUdp server(serverAddr);

  std::cout << "waiting for receipt at " << to_string(serverAddr)
            << std::endl;

  char buffer[256];
  if(auto rx = server.ReceiveFrom(buffer, sizeof(buffer), std::chrono::seconds(1))) {
    auto &&[receiveSize, fromAddr] = *rx;
    if(receiveSize == 0U) {
      std::cout << "received <empty> from " << to_string(fromAddr)
                << " responding with 'hello?'" << std::endl;

      for(int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        static char const hello[] = "hello?";
        (void)server.SendTo(hello, sizeof(hello), fromAddr);
      }
      return;
    }
  }

  throw std::runtime_error("failed to receive");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddr)
try {
  SocketUdp client(Address("localhost"));
  auto const clientAddr = client.LocalAddress();

  char buffer[256];
  if(client.ReceiveFrom(buffer, sizeof(buffer), std::chrono::milliseconds(100))) {
    throw std::runtime_error("unexpected receive");
  }

  std::cout << "sending <empty> from "
            << to_string(clientAddr) << " to "
            << to_string(serverAddr) << std::endl;

  for(int i = 0; i < 3; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    (void)client.SendTo(nullptr, 0U, serverAddr);
  }

  if(auto rx = client.ReceiveFrom(buffer, sizeof(buffer), std::chrono::seconds(1))) {
    auto &&[receiveSize, fromAddr] = *rx;
    if(std::string_view(buffer, receiveSize).find("hello?") != std::string_view::npos) {
      std::cout << "received 'hello?' from " << to_string(fromAddr) << std::endl;
      return;
    }
  }

  throw std::runtime_error("failed to receive response");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
try {
  Address serverAddr("localhost:8554");

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
