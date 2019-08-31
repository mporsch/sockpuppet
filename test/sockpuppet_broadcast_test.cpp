#include "socket.h" // for SocketUdp

#include <algorithm> // for std::for_each
#include <atomic> // for std::atomic
#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

using namespace sockpuppet;

static std::atomic<bool> success(true);

void Server(SocketAddress serverAddr)
try {
  SocketUdp server(serverAddr);

  std::cout << "receiving at " << to_string(serverAddr) << std::endl;

  char buffer[256];
  auto received = server.Receive(buffer, sizeof(buffer),
                                 std::chrono::seconds(1));
  if(received > 0U &&
     std::string(buffer, received).find("hello") != std::string::npos) {
    auto const t = server.ReceiveFrom(buffer, sizeof(buffer),
                                      std::chrono::seconds(1));
    received = std::get<0>(t);
    auto &&clientAddr = std::get<1>(t);

    if(received > 0U &&
       std::string(buffer, received).find("hello") != std::string::npos) {
      std::cout << "received from " << to_string(clientAddr) << std::endl;
      return;
    }
  }

  throw std::runtime_error("failed to receive");
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddr)
try {
  auto client = SocketUdp(serverAddr.Host());
  auto clientAddr = client.LocalAddress();
  auto broadcastAddr = SocketAddress(
        client.BroadcastAddress().Host(),
        serverAddr.Service());

  std::cout << "sending from "
    << to_string(clientAddr) << " to "
    << to_string(serverAddr) << " via broadcast "
    << to_string(broadcastAddr) << std::endl;

  for(int i = 0; i < 3; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    static char const hello[] = "hello";
    client.SendTo(hello, sizeof(hello), broadcastAddr);
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
try {
  auto localAddrs = SocketAddress::LocalAddresses();

  std::cout << "local interfaces:\n";
  std::for_each(
        std::begin(localAddrs), std::end(localAddrs),
        [](SocketAddress const &localAddr) {
    std::cout << std::setw(40)
              << to_string(localAddr)
              << " <-- " << (localAddr.IsV6()  ? "IPv6" : "IPv4")
              << std::endl;
  });
  std::cout << std::endl;

  std::for_each(
        std::begin(localAddrs), std::end(localAddrs),
        [](SocketAddress const &localAddr) {
    if (!localAddr.IsV6()) {
      SocketAddress serverAddr(localAddr.Host(), "8554");

      std::thread server(Server, serverAddr);
      std::thread client(Client, serverAddr);

      if(server.joinable()) {
        server.join();
      }
      if(client.joinable()) {
        client.join();
      }
    }
  });

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
