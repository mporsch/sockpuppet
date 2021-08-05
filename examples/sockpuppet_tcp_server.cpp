#include "sockpuppet/socket_async.h" // for SocketTcpAsyncServer

#include <csignal> // for std::signal
#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <map> // for std::map

using namespace sockpuppet;

// socket driver to run multiple client connections in one thread
static Driver driver;

// storage for connected client sockets
static std::map<Address, SocketTcpAsyncClient> clients;

void HandleSignal(int)
{
  driver.Stop();
}

void HandleReceive(BufferPtr buffer)
{
  // print whatever has just been received
  std::cout << *buffer << std::endl;
}

void HandleDisconnect(Address clientAddr)
{
  std::cout << "connection "
            << to_string(clientAddr)
            << " <- "
            << to_string(clients.at(clientAddr).LocalAddress())
            << " disconnected"
            << std::endl;

  // destroying the client socket closes the connection
  clients.erase(clientAddr);
}

void HandleConnect(SocketTcpClient clientSock, Address clientAddr)
{
  std::cout << "connection "
            << to_string(clientAddr)
            << " <- "
            << to_string(clientSock.LocalAddress())
            << " accepted"
            << std::endl;

  // augment the client socket to be an asynchronous one
  // attached to the same driver as the server socket
  SocketTcpAsyncClient clientAsync(
        SocketTcpBuffered(std::move(clientSock)),
        driver,
        HandleReceive,
        HandleDisconnect);

  // store the augmented client socket
  // (letting it go out of scope would otherwise close it immediately)
  (void)clients.emplace(std::make_pair(std::move(clientAddr), std::move(clientAsync)));
}

void Server(Address bindAddress)
{
  // set up the handler for Ctrl-C
  if(std::signal(SIGINT, HandleSignal) == SIG_ERR) {
    throw std::logic_error("failed to set signal handler");
  }

  // bind a TCP server socket to given address
  SocketTcpAsyncServer server(
      SocketTcpServer(bindAddress),
      driver,
      HandleConnect);

  // print the bound TCP socket address
  // (might have OS-assigned port number if
  // it has not been explicitly set in the bind address)
  std::cout << "listening at "
            << to_string(server.LocalAddress())
            << std::endl;

  // listen for, accept and serve incoming connections until Ctrl-C
  driver.Run();
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " SOURCE\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    // parse given address string
    Address bindAddress(argv[1]);

    // create and run a TCP server socket
    Server(bindAddress);
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
