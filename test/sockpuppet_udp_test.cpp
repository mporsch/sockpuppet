#include "sockpuppet/socket.h" // for SocketUdp

#include <atomic> // for std::atomic
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <stdexcept> // for std::runtime_error
#include <string_view> // for std::string_view
#include <thread> // for std::thread

using namespace sockpuppet;
using namespace std::chrono_literals;

static std::atomic<bool> success(true);

void Server(SocketUdp serverSock)
try {
  std::cout << "waiting for receipt at "
            << to_string(serverSock.LocalAddress())
            << std::endl;

  char buffer[256];
  constexpr Duration receiveTimeout = 1s;
  if(auto rx = serverSock.ReceiveFrom(buffer, sizeof(buffer), receiveTimeout)) {
    auto &&[receiveSize, fromAddr] = *rx;
    if(receiveSize == 0U) {
      std::cout << "received <empty> from " << to_string(fromAddr)
                << " responding with 'hello?'" << std::endl;

      for(int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(100ms);

        static char const hello[] = "hello?";
        (void)serverSock.SendTo(hello, sizeof(hello), fromAddr);
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
  auto clientSock = SocketUdp(Address());
  auto clientAddr = clientSock.LocalAddress();

  char buffer[256];
  constexpr Duration unexpectedReceiveTimeout = 100ms;
  if(clientSock.ReceiveFrom(buffer, sizeof(buffer), unexpectedReceiveTimeout)) {
    throw std::runtime_error("unexpected receive");
  }

  std::cout << "sending <empty> from "
            << to_string(clientAddr) << " to "
            << to_string(serverAddr) << std::endl;

  for(int i = 0; i < 3; ++i) {
    std::this_thread::sleep_for(100ms);

    (void)clientSock.SendTo(nullptr, 0U, serverAddr);
  }

  constexpr Duration expectedReceiveTimeout = 1s;
  if(auto rx = clientSock.ReceiveFrom(buffer, sizeof(buffer), expectedReceiveTimeout)) {
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
  auto serverSock = SocketUdp(Address());
  auto serverAddr = serverSock.LocalAddress();

  std::thread server(Server, std::move(serverSock));
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
