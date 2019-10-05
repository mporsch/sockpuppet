#include "sockpuppet_test_common.h" // for TestData
#include "socket_buffered.h" // for SocketTcpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::runtime_error
#include <thread> // for std::thread

using namespace sockpuppet;

static TestData const testData(10000000U);
static std::atomic<bool> success(true);

void ServerHandler(std::tuple<SocketTcpClient, SocketAddress> t)
{
  SocketTcpBuffered serverHandler(
        std::move(std::get<0>(t)), 0U, 1500U);

  std::vector<SocketUdpBuffered::SocketBufferPtr> storage;

  // receive until disconnect
  try {
    for(;;) {
      storage.emplace_back(serverHandler.Receive());
    }
  } catch(std::exception const &) {
  }

  if(!testData.Verify(storage)) {
    success = false;
  }
}

void Server(SocketAddress serverAddress)
try {
  SocketTcpServer server(serverAddress);

  std::cout << "server listening at " << to_string(serverAddress)
            << std::endl;

  ServerHandler(server.Listen());
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddress)
try {
  SocketTcpBuffered client(serverAddress);

  if(client.Receive(std::chrono::seconds(0))->size() != 0U) {
    throw std::runtime_error("unexpected receive");
  }

  std::cout << "client " << to_string(client.LocalAddress())
            << " connected to server " << to_string(serverAddress)
            << std::endl;

  testData.Send(client);

  // destroying the client socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  // start client and server threads
  SocketAddress serverAddr("localhost:8554");
  std::thread server(Server, serverAddr);

  // wait for server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::thread client(Client, serverAddr);

  // wait for both to finish
  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
