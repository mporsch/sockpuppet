#include "sockpuppet/address.h" // for Address

#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr

using namespace sockpuppet;

template<typename... Args>
void Test(Args&&... args)
{
  Address addr(std::forward<Args>(args)...);

  std::cout << std::setw(20)
            << to_string(addr)
            << " <-- host=\"" << addr.Host() << "\""
            << ", service=\"" << addr.Service() << "\""
            << ", " << (addr.IsV6()  ? "IPv6" : "IPv4")
            << std::endl;
}

int main(int, char **)
try {
  // localhost IPv4, OS-assigned port number
  Test();

  // localhost IPv4, fixed port number
  Test(554);

  // localhost URI, OS-assigned port number
  Test("localhost");

  // localhost URI with port/protocol
  Test("localhost:554");
  Test("http://localhost");

  // localhost host with port/protocol
  Test("localhost", "554");
  Test("localhost", "http");

  // IPv4 URI without port/protocol
  Test("91.198.174.192");

  // IPv4 URI with port/protocol
  Test("http://91.198.174.192");
  Test("91.198.174.192:80");

  // IPv4 URI with port/protocol and path
  Test("https://91.198.174.192/wiki/Wikipedia:Hauptseite");
  Test("91.198.174.192:443/wiki/Wikipedia:Hauptseite");

  // IPv4 host with port/protocol
  Test("91.198.174.192", "http");
  Test("91.198.174.192", "80");

  // IPv6 URI without port/protocol
  Test("::1");
  Test("a:b::c:1");

  // IPv6 URI with port/protocol
  Test("[::1]:554");
  Test("[a:b::c:1]:554");
  Test("http://::1");
  Test("http://a:b::c:1");

  // IPv6 URI with port/protocol and path
  Test("https://::/wiki/Wikipedia:Hauptseite");
  Test("[::]:443/wiki/Wikipedia:Hauptseite");

  // IPv6 host without port/protocol
  Test("::1", "554");
  Test("a:b::c:1", "554");
  Test("::1", "http");
  Test("a:b::c:1", "http");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
