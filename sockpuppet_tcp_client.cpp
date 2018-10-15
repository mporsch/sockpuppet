#include "socket.h" // for SocketTcpClient

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " DESTINATION\n\n"
         "\tDESTINATION is an address string to connect to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress const serverAddr(argv[1]);
    SocketTcpClient client(serverAddr);

    std::cout << "connected to "
      << std::to_string(serverAddr) << std::endl;

    for(;;) {
      std::string line;
      std::cout << "message to send? (empty for exit) - ";
      std::getline(std::cin, line);
      if(line.empty()) {
        break;
      } else {
        client.Send(line.c_str(), line.size());
      }
    }

    std::cout << "closing connection to "
      << std::to_string(serverAddr) << std::endl;
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
