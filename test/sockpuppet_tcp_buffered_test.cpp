#include "sockpuppet_test_common.h" // for TestData

#include "sockpuppet/socket_buffered.h" // for SocketTcpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::exception
#include <thread> // for std::thread

using namespace sockpuppet;
using namespace std::chrono_literals;

constexpr size_t testDataSize = 100U * 1024 * 1024;
static TestData const testData(testDataSize);
static std::atomic<bool> success(true);

void ServerHandler(std::pair<SocketTcp, Address> p)
{
  SocketTcpBuffered clientSock(std::move(p.first), 0U, 1500U);

  std::vector<BufferPtr> storage;
  storage.reserve(testDataSize / TestData::tcpPacketSizeMin);

  // receive until disconnect
  try {
    for(;;) {
      storage.emplace_back(clientSock.Receive().value());

      // simulate some processing delay to trigger TCP congestion control
      std::this_thread::sleep_for(100us);
    }
  } catch(std::exception const &) {
  }

  if(!testData.Verify(storage)) {
    success = false;
  }
}

void Server(Acceptor serverSock)
try {
  std::cout << "server listening at "
            << to_string(serverSock.LocalAddress())
            << std::endl;

  ServerHandler(serverSock.Listen().value());
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddress, Duration perPacketSendTimeout)
try {
  SocketTcpBuffered client(MakeTestSocket<SocketTcp>(serverAddress));

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
  auto serverSock = MakeTestSocket<Acceptor>(Address());
  auto serverAddr = serverSock.LocalAddress();

  // start client and server threads
  std::thread server(Server, std::move(serverSock));

  // wait for server to come up
  std::this_thread::sleep_for(1s);

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
  Test(Duration(1));

  std::cout << "test case #3: non-blocking send" << std::endl;
  Test(Duration(0));

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
