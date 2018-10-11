#include "socket_address.h" // for SocketAddress

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr

int main(int, char **)
try {
  SocketAddress();
  SocketAddress(8554);

  SocketAddress("localhost");
  SocketAddress("localhost:554");
  SocketAddress("rtsp://localhost");

  SocketAddress("::1");
  SocketAddress("a:b::c:1");
  SocketAddress("[::1]:554");
  SocketAddress("[a:b::c:1]:554");
  SocketAddress("rtsp://::1");
  SocketAddress("rtsp://a:b::c:1");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
