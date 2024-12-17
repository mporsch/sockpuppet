#include "sockpuppet_test_common.h" // for TestData
#include "sockpuppet/socket_buffered.h" // for SocketUdpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::exception
#include <thread> // for std::thread

using namespace sockpuppet;
using namespace std::chrono_literals;

constexpr size_t testDataSize = 100U * 1024;
static TestData const testData(testDataSize);
static std::atomic<bool> success(true);

void Server(SocketUdpBuffered serverSock)
try {
  std::cout << "waiting for receipt at "
            << to_string(serverSock.LocalAddress())
            << std::endl;

  std::vector<BufferPtr> storage;
  storage.reserve(testDataSize / TestData::udpPacketSize);

  // wait for first receipt
  {
    auto p = serverSock.ReceiveFrom().value();
    storage.emplace_back(std::move(p.first));

    std::cout << "receiving from "
              << to_string(p.second)
              << std::endl;
  }

  // receive until timeout
  constexpr Duration receiveTimeout = 100ms;
  while(auto rx = serverSock.ReceiveFrom(receiveTimeout)) {
    storage.emplace_back(std::move(rx->first));
  }

  if(!testData.Verify(storage)) {
    success = false;
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddr, Duration perPacketSendTimeout)
try {
  auto clientSock = SocketUdpBuffered(Address());

  testData.Send(clientSock, serverAddr, perPacketSendTimeout);
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Test(Duration perPacketSendTimeout)
{
  // start client and server threads
  auto serverSock = SocketUdpBuffered(Address(), 0U, 1500U);
  auto serverAddr = serverSock.LocalAddress();

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
