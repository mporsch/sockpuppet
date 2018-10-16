#include "socket.h" // for SocketTcpServer

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string
#include <thread> // for std::thread

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
    << std::to_string(clientAddr) << std::endl;

  handler.Send(response.c_str(), response.size());

  // destroying the handler socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
}

int main(int, char **)
try {
  SocketAddress const serverAddr("localhost:8080");
  SocketTcpServer server(serverAddr);

  std::cout << "listening for HTTP requests at "
    << std::to_string(serverAddr) << std::endl;

  // fire and forget handler threads for each incoming client
  // do not do this in your application!
  for(;;) {
    std::thread(ServerHandler, server.Listen()).detach();
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
