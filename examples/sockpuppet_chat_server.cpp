#include "sockpuppet/socket_async.h" // for SocketTcpAsyncServer

#include <csignal> // for std::signal
#include <cstdlib> // for EXIT_SUCCESS
#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <map> // for std::map

using namespace sockpuppet;

// socket driver to run multiple client connections in one thread
static Driver driver;

void HandleSignal(int)
{
  driver.Stop();
}

struct ChatServer
{
  SocketTcpAsyncServer server;

  // storage for connected client connection sockets
  std::map<Address, SocketTcpAsyncClient> clients;

  // send buffer pool
  BufferPool pool;

  // bind a TCP server socket to given address
  // (you can turn this into a TLS-encrypted server
  // by adding arguments for certificate and key file path)
  ChatServer(Address bindAddress)
    : server({bindAddress},
             driver,
             std::bind(&ChatServer::HandleConnect,
                       this,
                       std::placeholders::_1,
                       std::placeholders::_2))
  {
    // print the bound TCP socket address
    // (might have OS-assigned port number if
    // it has not been explicitly set in the bind address)
    std::cout << "listening at "
              << to_string(server.LocalAddress())
              << std::endl;
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
          {std::move(clientSock)},
          driver,
          std::bind(&ChatServer::HandleReceive, this, clientAddr, std::placeholders::_1),
          std::bind(&ChatServer::HandleDisconnect, this, std::placeholders::_1));

    // store the augmented client socket
    // (going out of scope would otherwise close it immediately)
    (void)clients.emplace(std::make_pair(std::move(clientAddr), std::move(clientAsync)));
  }

  void HandleReceive(Address clientAddr, BufferPtr receiveBuffer)
  {
    auto prefixed = to_string(clientAddr) + " says: " + *receiveBuffer;

    // print whatever has just been received
    std::cout << prefixed << std::endl;

    // forward to all but source client
    for(auto &&client : clients) {
      if(client.first != clientAddr) {
        auto sendBuffer = pool.Get();
        *sendBuffer = prefixed;
        (void)client.second.Send(std::move(sendBuffer));
        // TODO keep history and send to new clients on connect
      }
    }
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
    (void)clients.erase(clientAddr);
  }
};


void Server(Address bindAddress)
{
  // set up the handler for Ctrl-C
  if(std::signal(SIGINT, HandleSignal) == SIG_ERR) {
    throw std::logic_error("failed to set signal handler");
  }

  // create a server socket
  ChatServer server(bindAddress);

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
