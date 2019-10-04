#include "sockpuppet_test_common.h" // for TestData
#include "socket_buffered.h" // for SocketUdpBuffered

#include <atomic> // for std::atomic
#include <iostream> // for std::cout
#include <stdexcept> // for std::runtime_error
#include <thread> // for std::thread

using namespace sockpuppet;

static TestData const testData(1000000U);
static std::atomic<bool> success(true);

void Server(SocketAddress serverAddress)
try {
  SocketUdpBuffered server(serverAddress, 0U, 1500U);

  std::vector<SocketUdpBuffered::SocketBufferPtr> storage;

  // wait for first receipt
  {
    auto t = server.ReceiveFrom();
    storage.emplace_back(std::move(std::get<0>(t)));

    std::cout << "receiving from "
              << to_string(std::get<1>(t)) << std::endl;
  }

  // receive until timeout
  do {
    storage.emplace_back(
          server.Receive(
            std::chrono::milliseconds(100)));
  } while(storage.back()->size() > 0U);
  storage.pop_back();

  if(!testData.Verify(storage)) {
    success = false;
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddress)
try {
  SocketAddress clientAddress("localhost");
  SocketUdpBuffered client(clientAddress);

  if((client.Receive(std::chrono::seconds(0))->size() != 0U) ||
     (std::get<0>(client.ReceiveFrom(std::chrono::seconds(0)))->size() != 0U)){
    throw std::runtime_error("unexpected receive");
  }

  testData.Send(client, serverAddress);
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
