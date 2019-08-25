#include "socket_address.h" // for SocketAddress

#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr

using namespace sockpuppet;

template<typename... Args>
void Test(Args&&... args)
{
  SocketAddress socketAddress(std::forward<Args>(args)...);

  std::cout << std::setw(20)
            << to_string(socketAddress)
            << " <-- host=\"" << socketAddress.Host() << "\""
            << ", service=\"" << socketAddress.Service() << "\""
            << ", " << (socketAddress.IsV6()  ? "IPv6" : "IPv4")
            << std::endl;
}

int main(int, char **)
try {
  Test();
  Test(554);

  Test("localhost");
  Test("localhost:554");
  Test("rtsp://localhost");

  Test("localhost", "554");
  Test("localhost", "rtsp");

  Test("::1");
  Test("a:b::c:1");
  Test("[::1]:554");
  Test("[a:b::c:1]:554");
  Test("rtsp://::1");
  Test("rtsp://a:b::c:1");

  Test("[::1]", "554");
  Test("[a:b::c:1]", "554");
  Test("::1", "rtsp");
  Test("a:b::c:1", "rtsp");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
