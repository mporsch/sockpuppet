#include "socket_buffered.h" // for SocketUdpBuffered

#include <algorithm> // for std::generate
#include <iostream> // for std::cout
#include <random> // for std::default_random_engine
#include <thread> // for std::thread

bool success = true;
std::vector<char> referenceData(10000000U);

void Server(SocketAddress serverAddress)
try {
  SocketUdpBuffered server(serverAddress, 0U, 1500U);

  std::vector<SocketUdpBuffered::SocketBufferPtr> storage;

  // wait for first receipt
  {
    auto t = server.ReceiveFrom();
    storage.emplace_back(std::move(std::get<0>(t)));

    std::cout << "server receiving from "
      << std::to_string(std::get<1>(t)) << std::endl;
  }

  // receive until timeout
  do {
    storage.emplace_back(
      server.Receive(
        std::chrono::milliseconds(100U)));
  } while(storage.back()->size() > 0U);
  storage.pop_back();

  std::cout << "server verifying received "
               "against reference data" << std::endl;

  size_t pos = 0;
  for(auto &&packet : storage) {
    if(std::equal(
        std::begin(*packet),
        std::end(*packet),
        referenceData.data() + pos,
        referenceData.data() + pos + packet->size())) {
      pos += packet->size();
    } else {
      success = false;
      std::cout << "error at byte " << pos << std::endl;
      break;
    }
  }
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddress)
try {
  SocketAddress clientAddress;
  SocketUdpBuffered client(clientAddress);

  std::cout << "client sending reference data to server "
    << std::to_string(serverAddress) << std::endl;

  // send in fixed packet sizes
  size_t pos = 0;
  static size_t const packetSize = 1400U;
  for(; pos + packetSize < referenceData.size(); pos += packetSize) {
    client.SendTo(
      referenceData.data() + pos,
      packetSize,
      serverAddress);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // send the remaining data not filling a whole packet
  client.SendTo(
    referenceData.data() + pos,
    referenceData.size() - pos,
    serverAddress);
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

int main(int, char **)
{
  std::cout << "generating random reference data" << std::endl;

  std::default_random_engine generator;
  std::uniform_int_distribution<> distribution(
    std::numeric_limits<char>::min(),
    std::numeric_limits<char>::max());
  auto gen = [&]() -> char {
    return distribution(generator);
  };
  std::generate(std::begin(referenceData),
                std::end(referenceData),
                gen);

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
