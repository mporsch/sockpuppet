#include "sockpuppet/socket.h" // for SocketTcpServer

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void ServerHandler(std::pair<SocketTcpClient, Address> p)
try {
  auto &&clientSock = p.first;
  auto &&clientAddr = p.second;

  std::cout << "connection "
            << to_string(clientAddr)
            << " <- "
            << to_string(clientSock.LocalAddress())
            << " accepted"
            << std::endl;

  // receive and print until Ctrl-C or client disconnect
  for(;;) {
    char buffer[256];
    static Socket::Duration const noTimeout(-1);

    // wait for and receive incoming data into provided buffer
    // negative timeout -> blocking until receipt
    size_t received = clientSock.Receive(buffer,
                                         sizeof(buffer),
                                         noTimeout);

    // print whatever has just been received
    std::cout << std::string(buffer, received) << std::endl;
  }
} catch (std::exception const &e) {
  // (most probably) client disconnected
  std::cerr << e.what() << std::endl;
}

void Server(Address bindAddress)
{
  // bind a TCP server socket to given address
  SocketTcpServer server(bindAddress);

  // listen for and accept incoming connections until Ctrl-C
  for(;;) {
    static Socket::Duration const noTimeout(-1);

    // print the bound TCP socket address
    // (might have OS-assigned port number if
    // it has not been explicitly set in the bind address)
    std::cout << "listening at "
              << to_string(server.LocalAddress())
              << std::endl;

    // wait for and accept incoming connections
    // negative timeout -> blocking until connection
    ServerHandler(server.Listen(noTimeout));
  }
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " SOURCE\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    // parse given address string
    Address bindAddress(argv[1]);

    // create and run a TCP server socket
    Server(bindAddress);
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
