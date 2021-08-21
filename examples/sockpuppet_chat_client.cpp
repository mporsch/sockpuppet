#include "sockpuppet_chat_io_print.h" // for IOPrintBuffer

#include "sockpuppet/socket_async.h" // for SocketTcpAsync

#include <cstdlib> // for EXIT_SUCCESS
#include <functional> // for std::bind
#include <iostream> // for std::cout
#include <thread> // for std::thread

using namespace sockpuppet;

struct ReconnectClient
{
  Driver &driver;
  IOPrintBuffer &ioBuf;
  std::optional<SocketTcpAsync> client = std::nullopt;

  ReconnectClient(Address remoteAddress, Driver &driver, IOPrintBuffer &ioBuf)
    : driver(driver)
    , ioBuf(ioBuf)
  {
    // delay initial connect to get the order of prints right
    ToDo(driver,
         [=]() { Reconnect(remoteAddress); },
         std::chrono::milliseconds(500));
  }

  void Reconnect(Address remoteAddress, Duration delay = std::chrono::seconds(1))
  {
    try {
      // connect a TCP client socket to given address
      // (you can connect to a TLS-encrypted server
      // by adding arguments for certificate and key file path)
      client.emplace(
          SocketTcp(remoteAddress),
          driver,
          std::bind(&ReconnectClient::HandleReceive, this, std::placeholders::_1),
          std::bind(&ReconnectClient::HandleDisconnect, this, std::placeholders::_1));

      // print the bound TCP socket address
      // (might have OS-assigned interface and port number)
      // and remote address
      ioBuf.Print(
          "(re)established connection " +
          to_string(client->LocalAddress()) +
          " -> " +
          to_string(remoteAddress));
    } catch(std::exception const &) {
      ioBuf.Print(
          "failed to (re)connect to " +
          to_string(remoteAddress) +
          " will retry in " +
          std::to_string(delay.count()) +
          "s");

      // schedule a reconnect attempt with increasing backoff delay
      ToDo(driver,
           std::bind(&ReconnectClient::Reconnect, this, remoteAddress, delay * 2),
           delay);
    }
  }

  void Send(BufferPtr buffer)
  {
    if(client) {
      (void)client->Send(std::move(buffer));
    }
    // TODO cache failed send attempts and send after reconnect
  }

  void HandleReceive(BufferPtr buffer)
  {
    ioBuf.Print(*buffer);
  }

  void HandleDisconnect(Address remoteAddress)
  {
    ioBuf.Print(
        "closing connection " +
        to_string(client->LocalAddress()) +
        " -> " +
        to_string(remoteAddress));

    client.reset();
    Reconnect(remoteAddress);
  }
};

void Client(Address remoteAddress)
{
  // prepare print buffer that shows receipt history and allows user inputs
  IOPrintBuffer ioBuf(std::cout, 10U);

  // run socket in a separate thread as this one will be used for console input
  Driver driver;
  auto thread = std::thread(&Driver::Run, &driver);

  // create and connect client
  ReconnectClient client(remoteAddress, driver, ioBuf);

  // send buffer pool to be released after socket using it
  BufferPool pool;

  // query and send until cancelled
  for(;;) {
    // query a string to send from the command line
    auto line = pool.Get();
    *line = ioBuf.Query("message to send? (empty for exit) - ");

    if(line->empty()) {
      break;
    } else {
      ioBuf.Print("you said: " + *line);

      // enqueue the given string data to be sent to the connected peer
      (void)client.Send(std::move(line));
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
      << " DESTINATION\n\n"
         "\tDESTINATION is an address string to connect to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    // parse given address string
    Address remoteAddress(argv[1]);

    // create, connect and run a client socket
    Client(remoteAddress);
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
