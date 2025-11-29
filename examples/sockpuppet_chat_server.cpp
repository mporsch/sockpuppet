#include "sockpuppet_chat_io_print.h" // for IOPrintBuffer

#include "sockpuppet/socket_async.h" // for AcceptorAsync

#include <cstdlib> // for EXIT_SUCCESS
#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <thread> // for std::thread
#include <unordered_map> // for std::unordered_map

using namespace sockpuppet;

struct ChatServer
{
  AcceptorAsync server;
  Driver &driver;
  IOPrintBuffer &ioBuf;

  // storage for connected client connection sockets
  std::unordered_map<Address, SocketTcpAsync> clients;

  // send buffer pool
  BufferPool pool;

  // bind a TCP server socket to given address
  // (you can turn this into a TLS-encrypted server
  // by adding arguments for certificate and key file path)
  ChatServer(Address bindAddress, Driver &driver, IOPrintBuffer &ioBuf)
    : server({bindAddress},
             driver,
             std::bind(&ChatServer::HandleConnect,
                       this,
                       std::placeholders::_1,
                       std::placeholders::_2))
    , driver(driver)
    , ioBuf(ioBuf)
  {
    // print the bound TCP socket address
    // (might have OS-assigned port number if
    // it has not been explicitly set in the bind address)
    ioBuf.Print("listening at " + to_string(server.LocalAddress()));
  }

  void Send(std::string line)
  {
    // dispatch to socket driver
    ToDo(driver, [this, line = std::move(line)] {
      for(auto &&client : clients) {
        auto buffer = pool.Get();
        *buffer = line;
        (void)client.second.Send(std::move(buffer));
        // TODO keep history and send to new clients on connect
      }
    }, Duration(0));
  }

  void HandleConnect(SocketTcp clientSock, Address clientAddr)
  {
    ioBuf.Print("connection "
      + to_string(clientAddr)
      + " <- "
      + to_string(clientSock.LocalAddress())
      + " accepted");

    // augment the client socket to be an asynchronous one
    // attached to the same driver as the server socket
    SocketTcpAsync clientAsync(
          {std::move(clientSock)},
          driver,
          std::bind(&ChatServer::HandleReceive, this, clientAddr, std::placeholders::_1),
          std::bind(&ChatServer::HandleDisconnect, this, std::placeholders::_1));

    // store the augmented client socket
    // (going out of scope would otherwise close it immediately)
    (void)clients.emplace(std::move(clientAddr), std::move(clientAsync));
  }

  void HandleReceive(Address clientAddr, BufferPtr receiveBuffer)
  {
    auto prefixed = to_string(clientAddr) + " says: " + *receiveBuffer;

    // print whatever has just been received
    ioBuf.Print(prefixed);

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
    ioBuf.Print("connection "
      + to_string(clientAddr)
      + " <- "
      + to_string(clients.at(clientAddr).LocalAddress())
      + " disconnected");

    // destroying the client socket closes the connection
    (void)clients.erase(clientAddr);
  }
};

void Server(Address bindAddress)
{
  // socket driver to run multiple client connections in one thread
  Driver driver;

  // run sockets in a separate thread as this one will be used for console input
  auto thread = std::thread(&Driver::Run, &driver);

  // prepare print buffer that shows receipt history and allows user inputs
  IOPrintBuffer ioBuf(std::cout, 10U);

  // create a server socket to listen for, accept and serve incoming connections
  ChatServer server(bindAddress, driver, ioBuf);

  // query and send until cancelled
  for(;;) {
    // query a string to send from the command line
    auto line = ioBuf.Query("message to send? (empty for exit) - ");

    if(line.empty()) {
      break;
    } else {
      ioBuf.Print("you said: " + line);

      // enqueue the given string data to be sent to all the connected clients
      server.Send(std::move(line));
    }
  }

  driver.Stop();
  if(thread.joinable()) {
    thread.join();
  }
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
