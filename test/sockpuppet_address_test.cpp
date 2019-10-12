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
  Test("http://localhost");

  Test("localhost", "554");
  Test("localhost", "http");

  Test("::1");
  Test("a:b::c:1");
  Test("[::1]:554");
  Test("[a:b::c:1]:554");
  Test("http://::1");
  Test("http://a:b::c:1");

  Test("::1", "554");
  Test("a:b::c:1", "554");
  Test("::1", "http");
  Test("a:b::c:1", "http");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
