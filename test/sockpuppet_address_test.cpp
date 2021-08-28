#include "sockpuppet/address.h" // for Address

#include <algorithm> // for std::find
#include <cstdlib> // for EXIT_SUCCESS
#include <initializer_list> // for std::initializer_list
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr

using namespace sockpuppet;

struct Expected
{
  std::string host;
  std::string serv;
  bool isV6;

  bool operator==(Address const &addr) const
  {
    return ((host == addr.Host()) &&
            (serv == addr.Service()) &&
            (isV6 == addr.IsV6()));
  }
};

void Verify(std::initializer_list<Expected> expected, Address addr)
{
  if(std::find(std::begin(expected), std::end(expected), addr) == std::end(expected)) {
    throw std::runtime_error("constructed address does not match any reference");
  }
}

void Test(std::initializer_list<Expected> expected)
{
  Address addr;

  std::cout << std::setw(20)
            << to_string(addr)
            << " <-- "
            << "Address()"
            << std::endl;

  Verify(std::move(expected), std::move(addr));
}

void Test(std::initializer_list<Expected> expected, uint16_t port)
{
  Address addr(port);

  std::cout << std::setw(20)
            << to_string(addr)
            << " <-- "
            << "Address(" << port << ")"
            << std::endl;

  Verify(std::move(expected), std::move(addr));
}

void Test(std::initializer_list<Expected> expected, std::string uri)
{
  Address addr(uri);

  std::cout << std::setw(20)
            << to_string(addr)
            << " <-- "
            << "Address(\"" << uri << "\")"
            << std::endl;

  Verify(std::move(expected), std::move(addr));
}

void Test(std::initializer_list<Expected> expected, std::string host, std::string serv)
{
  Address addr(host, serv);

  std::cout << std::setw(20)
            << to_string(addr)
            << " <-- "
            << "Address(\"" << host << "\", \"" << serv << "\")"
            << std::endl;

  Verify(std::move(expected), std::move(addr));
}

int main(int, char **)
try {
  // localhost IPv4, OS-assigned port number
  Test({{"127.0.0.1", "0", false}});

  // localhost IPv4, fixed port number
  Test({{"127.0.0.1", "554", false}},
       uint16_t(554));

  // localhost URI, OS-assigned IPv4 or IPv6, OS-assigned port number
  Test({{"127.0.0.1", "0", false}, {"::1", "0", true}},
       "localhost");

  // localhost URI, OS-assigned IPv4 or IPv6, with port/protocol
  Test({{"127.0.0.1", "554", false}, {"::1", "554", true}},
       "localhost:554");
  Test({{"127.0.0.1", "80", false}, {"::1", "80", true}},
       "http://localhost");
  Test({{"127.0.0.1", "8080", false}, {"::1", "8080", true}},
       "http://localhost:8080");

  // localhost host, OS-assigned IPv4 or IPv6, with port/protocol
  Test({{"127.0.0.1", "554", false}, {"::1", "554", true}},
       "localhost", "554");
  Test({{"127.0.0.1", "80", false}, {"::1", "80", true}},
       "localhost", "http");

  // IPv4 URI without port/protocol
  Test({{"91.198.174.192", "0", false}},
       "91.198.174.192");

  // IPv4 URI with port/protocol
  Test({{"91.198.174.192", "80", false}},
       "91.198.174.192:80");
  Test({{"91.198.174.192", "80", false}},
       "http://91.198.174.192");
  Test({{"91.198.174.192", "8080", false}},
       "http://91.198.174.192:8080");

  // IPv4 URI with port/protocol and path
  Test({{"91.198.174.192", "8080", false}},
       "91.198.174.192:8080/wiki/Wikipedia:Hauptseite");
  Test({{"91.198.174.192", "80", false}},
       "http://91.198.174.192/wiki/Wikipedia:Hauptseite");
  Test({{"91.198.174.192", "8080", false}},
       "http://91.198.174.192:8080/wiki/Wikipedia:Hauptseite");

  // IPv4 host with port/protocol
  Test({{"91.198.174.192", "80", false}},
       "91.198.174.192", "80");
  Test({{"91.198.174.192", "80", false}},
       "91.198.174.192", "http");

  // IPv6 URI without port/protocol
  Test({{"::1", "0", true}},
       "::1");
  Test({{"a:b::c:1", "0", true}},
       "a:b::c:1");

  // IPv6 URI with port/protocol
  Test({{"::1", "554", true}},
       "[::1]:554");
  Test({{"a:b::c:1", "554", true}},
       "[a:b::c:1]:554");
  Test({{"::1", "80", true}},
       "http://::1");
  Test({{"a:b::c:1", "80", true}},
       "http://a:b::c:1");
  Test({{"::1", "8080", true}},
       "http://[::1]:8080");
  Test({{"a:b::c:1", "8080", true}},
       "http://[a:b::c:1]:8080");

  // IPv6 URI with port/protocol and path
  Test({{"::", "80", true}},
       "[::]:80/wiki/Wikipedia:Hauptseite");
  Test({{"::", "80", true}},
       "http://::/wiki/Wikipedia:Hauptseite");
  Test({{"::", "8080", true}},
       "http://[::]:8080/wiki/Wikipedia:Hauptseite");

  // IPv6 host without port/protocol
  Test({{"::1", "554", true}},
       "::1", "554");
  Test({{"a:b::c:1", "8080", true}},
       "a:b::c:1", "8080");
  Test({{"::1", "80", true}},
       "::1", "http");
  Test({{"a:b::c:1", "80", true}},
       "a:b::c:1", "http");

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
