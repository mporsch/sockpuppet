#include "socket_buffered.h" // for SocketTcpBuffered

#include <algorithm> // for std::generate
#include <iostream> // for std::cout
#include <random> // for std::default_random_engine
#include <thread> // for std::thread

using namespace sockpuppet;

bool success = true;
std::vector<char> referenceData(10000000U);

void ServerHandler(std::tuple<SocketTcpClient, SocketAddress> t)
{
  SocketTcpBuffered serverHandler(
    std::move(std::get<0>(t)), 0U, 1500U);

  std::vector<SocketUdpBuffered::SocketBufferPtr> storage;

  std::cout << "server receiving from "
    << to_string(std::get<1>(t)) << std::endl;

  // receive until disconnect
  try {
    for(;;) {
      storage.emplace_back(serverHandler.Receive());
    }
  } catch(std::exception const &) {
  }

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
}

void Server(SocketAddress serverAddress)
try {
  SocketTcpServer server(serverAddress);

  ServerHandler(server.Listen());
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  success = false;
}

void Client(SocketAddress serverAddress)
try {
  SocketTcpBuffered client(serverAddress);

  std::cout << "client sending reference data to server "
    << to_string(serverAddress) << std::endl;

  // send in randomly sized packets
  std::default_random_engine generator;
  std::uniform_int_distribution<size_t> distribution(100U, 10000U);
  auto gen = [&]() -> size_t {
    return distribution(generator);
  };

  size_t pos = 0;
  auto packetSize = gen();
  while(pos + packetSize < referenceData.size()) {
    client.Send(
      referenceData.data() + pos,
      packetSize);
    pos += packetSize;
    packetSize = gen();
  }

  // send the remaining data not filling a whole packet
  client.Send(
    referenceData.data() + pos,
    referenceData.size() - pos);

  // destroying the client socket closes the connection
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
