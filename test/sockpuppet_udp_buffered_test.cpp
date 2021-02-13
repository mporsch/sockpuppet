#include "sockpuppet_test_common.h" // for TestData
#include "sockpuppet/socket_buffered.h" // for SocketUdpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::exception
#include <thread> // for std::thread

using namespace sockpuppet;

static TestData const testData(100000U);
static std::atomic<bool> success(true);

void Server(Address serverAddress)
try {
  SocketUdpBuffered server(serverAddress, 0U, 1500U);

  std::cout << "waiting for receipt at " << to_string(serverAddress)
            << std::endl;

  std::vector<BufferPtr> storage;

  // wait for first receipt
  {
    auto t = server.ReceiveFrom();
    storage.emplace_back(std::move(std::get<0>(t)));

    std::cout << "receiving from "
              << to_string(std::get<1>(t))
              << std::endl;
  }

  // receive until timeout
  do {
    storage.emplace_back(
          server.ReceiveFrom(
            std::chrono::milliseconds(100)).first);
  } while(storage.back()->size() > 0U);
  storage.pop_back();

  if(!testData.Verify(storage)) {
    success = false;
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(Address serverAddress, Duration perPacketSendTimeout)
try {
  Address clientAddress("localhost");
  SocketUdpBuffered client(clientAddress);

  testData.Send(client, serverAddress, perPacketSendTimeout);
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
