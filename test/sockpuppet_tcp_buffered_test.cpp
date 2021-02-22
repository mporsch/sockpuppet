#include "sockpuppet_test_common.h" // for TestData
#include "sockpuppet/socket_buffered.h" // for SocketTcpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::exception
#include <thread> // for std::thread

using namespace sockpuppet;

static TestData const testData(10000000U);
static std::atomic<bool> success(true);

void ServerHandler(std::pair<SocketTcpClient, Address> p)
{
  SocketTcpBuffered serverHandler(std::move(p.first), 0U, 1500U);

  std::vector<BufferPtr> storage;

  // receive until disconnect
  try {
    for(;;) {
      storage.emplace_back(serverHandler.Receive().value());

      // simulate some processing delay to trigger TCP congestion control
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  } catch(std::exception const &) {
  }

  if(!testData.Verify(storage)) {
    success = false;
  }
}

void Server(Address serverAddress)
try {
  SocketTcpServer server(serverAddress);

  std::cout << "server listening at " << to_string(serverAddress)
            << std::endl;

  ServerHandler(server.Listen().value());
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddress, Duration perPacketSendTimeout)
try {
  SocketTcpBuffered client(serverAddress);

  std::cout << "client " << to_string(client.LocalAddress())
            << " connected to server " << to_string(serverAddress)
            << std::endl;

  testData.Send(client, perPacketSendTimeout);

  // destroying the client socket closes the connection
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Test(Duration perPacketSendTimeout)
{
  // start client and server threads
  Address serverAddr("localhost:8554");
  std::thread server(Server, serverAddr);

  // wait for server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::thread client(Client, serverAddr, perPacketSendTimeout);

  // wait for both to finish
  if(server.joinable()) {
    server.join();
  }
  if(client.joinable()) {
    client.join();
  }
}

int main(int, char **)
{
  std::cout << "test case #1: unlimited send timeout" << std::endl;
  Test(Duration(-1));

  std::cout << "test case #2: limited send timeout" << std::endl;
  Test(std::chrono::milliseconds(1));

  std::cout << "test case #3: non-blocking send" << std::endl;
  Test(Duration(0));

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
