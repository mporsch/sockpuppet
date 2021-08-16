#include "sockpuppet/socket_async.h" // for SocketUdpAsync
#include "../src/address_impl.h" // for Address::impl
#include "../src/socket_async_impl.h" // for SocketUdpAsync::impl

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout

using namespace sockpuppet;

void HandleReceiveFrom(BufferPtr, Address)
{
}

int main(int argc, char **argv)
{
  Address addr;
  if(argc > 1) {
    addr = Address(argv[1]);
  }
  Driver driver;
  SocketUdpAsync sock({addr}, driver, HandleReceiveFrom);

  // the hidden implementation should be acessible after including the internal headers
  std::cout << "Family of address " << to_string(addr) << " is " << addr.impl->Family()
            << std::endl
            << "File descriptor of bound socket is " << sock.impl->buff->sock->fd
            << " (" << sizeof(sock.impl->buff->sock->fd) << " byte integer)"
            << std::endl;

  return EXIT_SUCCESS;
}
