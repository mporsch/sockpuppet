#include "socket_address.h" // for SocketAddress

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr

template<typename... Args>
void Test(Args&&... args)
{
  SocketAddress socketAddress(std::forward<Args>(args)...);
  std::cout << std::to_string(socketAddress) << std::endl;
}

int main(int, char **)
try {
  Test();
  Test(554);

  Test("localhost");
  Test("localhost:554");
  Test("rtsp://localhost");

  Test("::1");
  Test("a:b::c:1");
  Test("[::1]:554");
  Test("[a:b::c:1]:554");
  Test("rtsp://::1");
  Test("rtsp://a:b::c:1");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
