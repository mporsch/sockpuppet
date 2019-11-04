#include "sockpuppet/socket.h" // for SocketTcpClient

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void Client(Address remoteAddress)
{
  // connect a TCP client socket to given address
  SocketTcpClient client(remoteAddress);

  // print the bound TCP socket address
  // (has OS-assigned interface and port number)
  // and remote address
  std::cout << "established connection "
            << to_string(client.LocalAddress())
            << " -> "
            << to_string(remoteAddress)
            << std::endl;

  // query and send until cancelled
  for(;;) {
    // query a string to send from the command line
    std::string line;
    std::cout << "message to send? (empty for exit) - ";
    std::getline(std::cin, line);

    if(line.empty()) {
      break;
    } else {
      static Socket::Duration const noTimeout(-1);

      // send the given string data to the connected peer
      // negative timeout -> blocking until sent
      // ignore the return value as - with unlimited timeout -
      // it will always match the sent size
      (void)client.Send(line.c_str(),
                        line.size(),
                        noTimeout);
    }
  }

  std::cout << "closing connection "
            << to_string(client.LocalAddress())
            << " -> "
            << to_string(remoteAddress)
            << std::endl;
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " DESTINATION\n\n"
         "\tDESTINATION is an address string to connect to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    // parse given address string
    Address remoteAddress(argv[1]);

    // create, connect and run a TCP client socket
    Client(remoteAddress);
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
