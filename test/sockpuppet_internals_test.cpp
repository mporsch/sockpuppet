#include "sockpuppet/socket_async.h" // for SocketUdpAsync
#include "../src/address_priv.h" // for Address::priv
#include "../src/socket_async_priv.h" // for SocketUdpAsync::priv

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout

using namespace sockpuppet;

void HandleReceive(BufferPtr)
{
}

int main(int argc, char **argv)
{
  Address addr;
  if(argc > 1) {
    addr = Address(argv[1]);
  }
  SocketDriver driver;
  SocketUdpAsync sock({addr}, driver, HandleReceive);

  // the hidden implementation should be acessible after including the internal headers
  std::cout << "Family of address " << to_string(addr) << " is " << addr.priv->Family()
            << std::endl
            << "File descriptor of bound socket is " << sock.priv->fd
            << " (" << sizeof(sock.priv->fd) << " byte integer)"
            << std::endl;

  return EXIT_SUCCESS;
}
