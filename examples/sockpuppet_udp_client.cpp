#include "sockpuppet/socket.h" // for SocketUdp

#include <atomic> // for std::atomic
#include <csignal> // for std::signal
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string> // for std::string

using namespace sockpuppet;

void Client(Address bindAddress, Address remoteAddress)
{
  // set up the handler for Ctrl-C
  static std::atomic<bool> shouldStop = false;
  auto signalHandler = [](int) {
    shouldStop = true;
  };
  if(std::signal(SIGINT, signalHandler) == SIG_ERR) {
    throw std::logic_error("failed to set signal handler");
  }

  // bind a UDP socket to given address
  SocketUdp sock(bindAddress);

  // print the bound UDP socket address
  // (might have OS-assigned port number if
  // it has not been explicitly set in the bind address)
  // and remote address
  std::cout << "sending from "
            << to_string(sock.LocalAddress())
            << " to "
            << to_string(remoteAddress)
            << std::endl;

  // query and send until cancelled
  while(!shouldStop) {
    // query a string to send from the command line
    std::string line;
    std::cout << "message to send? (Ctrl-C for exit) - ";
    std::getline(std::cin, line);

    // send the given string data to the remote address
    // negative timeout -> blocking until sent (although
    // UDP sockets will rarely ever block on send)
    // ignore the return value as - with unlimited timeout -
    // it will always match the sent size
    static Duration const noTimeout(-1);
    (void)sock.SendTo(line.c_str(),
                      line.size(),
                      remoteAddress,
                      noTimeout);
  }
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0]
      << " DESTINATION [SOURCE]\n\n"
         "\tDESTINATION is an address string to send to\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
    return EXIT_FAILURE;
  }

  // parse given address string(s)
  Address remoteAddress(argv[1]);
  Address bindAddress;
  if(argc >= 3) {
    bindAddress = Address(argv[2]);
  }

  // create and run a UDP socket
  Client(bindAddress, remoteAddress);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
