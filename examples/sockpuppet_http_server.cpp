#include "sockpuppet/socket_async.h" // for AcceptorAsync

#include <csignal> // for std::signal
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string
#include <vector> // for std::vector

using namespace sockpuppet;

namespace {

std::string const response =
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

void HandleConnect(SocketTcp clientSock, Address clientAddr)
try {
  // here we intentionally misuse the connect handler; instead of
  // only storing the client connection we do the whole HTTP handling and
  // immediately afterwards close the connection

  char buffer[256];
  while(clientSock.Receive(buffer, sizeof(buffer),
                           std::chrono::milliseconds(10))) {
    // simply keep receiving whatever the client sends
    // until we run into the timeout
    // assume this to be an HTTP GET
  }

  std::cout << "sending HTTP response to "
            << to_string(clientAddr) << std::endl;

  (void)clientSock.Send(response.c_str(), response.size());

  // destroying the handler socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
}

} // unnamed namespace

int main(int, char **)
try {
  // socket driver to run multiple servers in one thread
  static Driver driver;

  // set up the handler for Ctrl-C
  auto signalHandler = [](int) {
    driver.Stop();
  };
  if(std::signal(SIGINT, signalHandler) == SIG_ERR) {
    throw std::logic_error("failed to set signal handler");
  }

  // list the local machine interface addresses
  auto addrs = Address::LocalAddresses();

  // set the server port for each address
  for(auto &&addr : addrs) {
    addr = Address(addr.Host(), "8080");
  }

  // prepare a server for each interface address
  // (you can turn this into a TLS-encrypted server
  // by adding arguments for certificate and key file path)
  std::vector<AcceptorAsync> servers;
  {
    for(auto &&addr : addrs) {
      try {
        servers.emplace_back(
            Acceptor(addr),
            driver,
            HandleConnect);
      } catch(std::exception const &e) {
        // if binding one server fails, just go on
        std::cerr << e.what() << std::endl;
      }
    }
    if(servers.empty()) {
      throw std::runtime_error("failed to bind any server socket");
    }
  }

  std::cout << "listening for HTTP requests at:\n";
  for(auto &&server : servers) {
    std::cout << "  " << to_string(server.LocalAddress()) << "\n";
  }
  std::cout << "open any of these URLs in your web browser" << std::endl;

  // start the servers (blocking call, cancelled by Ctrl-C)
  driver.Run();

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
