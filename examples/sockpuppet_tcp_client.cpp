#include "sockpuppet/socket.h" // for SocketTcp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void Client(Address remoteAddress)
{
  // connect a TCP client socket to given address
  // (you can connect to a TLS-encrypted server
  // by adding arguments for certificate and key file path)
  SocketTcp client(remoteAddress);

  // print the bound TCP socket address
  // (might have OS-assigned interface and port number)
  // and remote address
  std::cout << "established connection "
            << to_string(client.LocalAddress())
            << " -> "
            << to_string(remoteAddress)
            << std::endl;

  // query and send until cancelled
  std::cerr << "message(s) to send? (Ctrl-C for exit)" << std::endl;

  // query strings to send from the command line or piped text (file) input
  std::string line;
  while(std::getline(std::cin, line)) {
    // as TCP cannot send empty lines, we always append a newline
    // the server example application is written accordingly
    line += '\n';

    // send the given string data to the connected peer
    // negative timeout -> blocking until sent
    // ignore the return value as - with unlimited timeout -
    // it will always match the sent size
    constexpr Duration noTimeout(-1);
    (void)client.Send(line.c_str(),
                      line.size(),
                      noTimeout);
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
    std::cerr << "Usage: " << argv[0]
      << " DESTINATION\n\n"
         "\tDESTINATION is an address string to connect to, "
         "e.g. \"localhost:8554\""
      << std::endl;
    return EXIT_FAILURE;
  }

  // parse given address string
  Address remoteAddress(argv[1]);

  // create, connect and run a TCP client socket
  Client(remoteAddress);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
