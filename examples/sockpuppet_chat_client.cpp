#include "sockpuppet/socket_async.h" // for SocketTcpAsyncClient

#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cout
#include <functional> // for std::bind
#include <thread> // for std::thread

using namespace sockpuppet;

static char const inputPrompt[] = "message to send? (empty for exit) - ";
static size_t const inputPromptLength = sizeof(inputPrompt) - 1;

void PrintInputPrompt()
{
  std::cout << inputPrompt << std::flush;
}

void PrintLine(std::string const &line)
{
  std::cout << '\r'
            << std::setw(inputPromptLength)
            << std::setfill(' ')
            << std::left
            << line
            << std::endl;
  PrintInputPrompt();
}

struct ReconnectClient
{
  Driver &driver;
  std::optional<SocketTcpAsyncClient> client = std::nullopt;

  void Reconnect(Address remoteAddress, Duration delay = std::chrono::seconds(1))
  {
    try {
      // connect a TCP client socket to given address
      // (you can connect to a TLS-encrypted server
      // by adding arguments for certificate and key file path)
      client.emplace(
          SocketTcpClient(remoteAddress),
          driver,
          std::bind(&ReconnectClient::HandleReceive, this, std::placeholders::_1),
          std::bind(&ReconnectClient::HandleDisconnect, this, std::placeholders::_1));

      // print the bound TCP socket address
      // (might have OS-assigned interface and port number)
      // and remote address
      std::cout << "(re)established connection "
                << to_string(client->LocalAddress())
                << " -> "
                << to_string(remoteAddress)
                << std::endl;
    } catch(std::exception const &) {
      std::cout << "failed to (re)connect to "
                << to_string(remoteAddress)
                << " will retry in "
                << delay.count() << "s "
                << std::endl;

      // schedule a reconnect attempt with increasing backoff delay
      ToDo(driver,
           std::bind(&ReconnectClient::Reconnect, this, remoteAddress, delay * 2),
           delay);
    }
  }

  void Send(BufferPtr buffer)
  {
    (void)client->Send(std::move(buffer));
    // TODO cache failed send attempts and send after reconnect
  }

  void HandleReceive(BufferPtr buffer)
  {
    // print whatever has just been received
    PrintLine(*buffer);
  }

  void HandleDisconnect(Address remoteAddress)
  {
    std::cout << "closing connection "
              << to_string(client->LocalAddress())
              << " -> "
              << to_string(remoteAddress)
              << std::endl;

    client.reset();
    Reconnect(remoteAddress);
  }
};

void Client(Address remoteAddress)
{
  // run socket in a separate thread as this one will be used for console input
  Driver driver;
  auto thread = std::thread(&Driver::Run, &driver);

  // send buffer pool to be released after socket using it
  BufferPool pool;

  ReconnectClient client{driver};
  client.Reconnect(remoteAddress);

  // query and send until cancelled
  for(;;) {

    // query a string to send from the command line
    PrintInputPrompt();
    auto line = pool.Get();
    std::getline(std::cin, *line);

    if(line->empty()) {
      break;
    } else {
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
