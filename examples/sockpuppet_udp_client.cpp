#include "socket.h" // for SocketUdp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <string> // for std::string

using namespace sockpuppet;

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " DESTINATION [SOURCE]\n\n"
         "\tDESTINATION is an address string to send to\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress const dstAddr(argv[1]);
    SocketAddress srcAddr;
    if(argc >= 3) {
      srcAddr = SocketAddress(argv[2]);
    }
    bool const isV4 = !srcAddr.IsV6();

    SocketUdp sock(srcAddr);

    std::cout << "sending from "
              << to_string(srcAddr) << " to "
              << to_string(dstAddr) << std::endl;

    for(;;) {
      std::string line;
      std::cout << "message to send? (empty for exit"
                << (isV4 ? ", prefix '!' for broadcast" : "")
                << ") - ";
      std::getline(std::cin, line);
      if(line.empty()) {
        break;
      } else {
        if(isV4 && line[0] == '!') {
          sock.SendTo(line.c_str() + 1, line.size(),
                      sock.BroadcastAddress(dstAddr.Port()));
        } else {
          sock.SendTo(line.c_str(), line.size(), dstAddr);
        }
      }
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
