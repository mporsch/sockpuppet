#include "socket.h" // for SocketTcpServer

#include <algorithm> // for std::transform
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <iterator> // for std::back_inserter
#include <string> // for std::string
#include <thread> // for std::thread
#include <vector> // for std::vector

using namespace sockpuppet;

static std::string const response =
  std::string("HTTP/1.1 200\r\nContent-Type: text/html\r\n\r\n")
  + R"(<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>It works!</title>
  </head>
  <body>
    <br>What did you expect?</br>
  </body>
</html>)";

void ServerHandler(std::tuple<SocketTcpClient, SocketAddress> t)
try {
  auto &&handler = std::get<0>(t);
  auto &&clientAddr = std::get<1>(t);

  char buffer[256];
  while(handler.Receive(buffer, sizeof(buffer),
                        std::chrono::milliseconds(10))) {
    // simply keep receiving whatever the client sends
    // until we run into the timeout
    // assume this to be an HTTP GET
  }

  std::cout << "sending HTTP response to "
    << to_string(clientAddr) << std::endl;

  handler.Send(response.c_str(), response.size());

  // destroying the handler socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
}

void Server(SocketAddress serverAddr)
try {
  SocketTcpServer server(serverAddr);

  // handle one client after the other
  for(;;) {
    ServerHandler(server.Listen());
  }
} catch(std::exception const &e) {
  std::cerr << e.what() << std::endl;
}

int main(int, char **)
try {
  // list the local machine interface addresses
  auto addrs = SocketAddress::LocalAddresses();

  // set the server port for each address
  for(auto &&addr : addrs) {
    addr = SocketAddress(addr.Host(), "8080");
  }

  std::cout << "listening for HTTP requests at:\n";
  for(auto &&addr : addrs) {
    std::cout << "  " << to_string(addr) << "\n";
  }
  std::cout << "open any of these URLs in your web browser" << std::endl;

  // start a server thread for each interface address
  std::vector<std::thread> threads;
  std::transform(
        std::begin(addrs), std::end(addrs),
        std::back_inserter(threads),
        [](SocketAddress const &addr) -> std::thread {
    return std::thread(Server, addr);
  });

  for(auto &&t : threads) {
    if(t.joinable()) {
      t.join();
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
