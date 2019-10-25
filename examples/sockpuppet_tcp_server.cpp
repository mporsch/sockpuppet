#include "sockpuppet/socket.h" // for SocketTcpServer

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void ServerHandler(std::pair<SocketTcpClient, Address> p)
try {
  auto &&clientSock = p.first;
  auto &&clientAddr = p.second;

  std::cout << "client " << to_string(clientAddr)
    << " connected" << std::endl;

  for(;;) {
    char buffer[256];
    auto const received = clientSock.Receive(buffer, sizeof(buffer));
    std::cout << std::string(buffer, received) << std::endl;
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
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
    Address const serverAddr(argv[1]);
    SocketTcpServer server(serverAddr);

    for(;;) {
      std::cout << "listening at " << to_string(server.LocalAddress())
                << std::endl;

      ServerHandler(server.Listen());
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
