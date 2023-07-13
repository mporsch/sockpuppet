#include "sockpuppet/socket.h" // for Acceptor

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void HandleConnect(std::pair<SocketTcp, Address> p)
try {
  auto &&clientSock = p.first;
  auto &&clientAddr = p.second;

  std::cerr << "connection "
            << to_string(clientAddr)
            << " <- "
            << to_string(clientSock.LocalAddress())
            << " accepted"
            << std::endl;

  // receive and print until Ctrl-C or client disconnect
  for(;;) {
    // wait for and receive incoming data into provided buffer
    // negative timeout -> blocking until receipt
    char buffer[256];
    constexpr Duration noTimeout(-1);
    size_t received = *clientSock.Receive(buffer,
                                          sizeof(buffer),
                                          noTimeout);

    // print whatever has just been received
    std::cout << std::string(buffer, received) << std::flush;
  }
} catch (std::exception const &e) {
  // (most probably) client disconnected
  std::cerr << e.what() << std::endl;
}

[[noreturn]] void Server(Address bindAddress)
{
  // bind a TCP server socket to given address
  // (you can turn this into a TLS-encrypted server
  // by adding arguments for certificate and key file path)
  Acceptor server(bindAddress);

  // listen for and accept incoming connections until Ctrl-C
  for(;;) {
    // print the bound TCP socket address
    // (might have OS-assigned interface and port number if
    // it has not been explicitly set in the bind address)
    std::cerr << "listening at "
              << to_string(server.LocalAddress())
              << std::endl;

    // wait for and accept incoming connections
    // negative timeout -> blocking until connection
    constexpr Duration noTimeout(-1);
    HandleConnect(*server.Listen(noTimeout));
  }
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0]
      << " SOURCE\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
    return EXIT_FAILURE;
  }

  // parse given address string
  Address bindAddress(argv[1]);

  // create and run a TCP server socket
  Server(bindAddress);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
