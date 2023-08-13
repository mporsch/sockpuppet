#include "sockpuppet/socket_async.h" // for SocketTcpAsync

#include <algorithm> // for std::generate
#include <cassert> // for assert
#include <iostream> // for std::cout
#include <random> // for std::default_random_engine
#include <string> // for std::string
#include <string_view> // for std::string_view
#include <thread> // for std::this_thread

namespace sockpuppet {

template<typename Socket, typename... Args>
Socket MakeTestSocket(Args&&... args)
{
#ifdef TEST_TLS
  return Socket(std::forward<Args>(args)..., "test_cert.pem", "test_key.pem");
#else // TEST_TLS
  return Socket(std::forward<Args>(args)...);
#endif // TEST_TLS
}

struct TestData
{
  static constexpr size_t udpPacketSize = 1400U;
  static constexpr size_t tcpPacketSizeMin = 100U;
  static constexpr size_t tcpPacketSizeMax = 10000U;

  std::string referenceData;

  TestData(size_t size)
    : referenceData(Generate(size))
  {
  }

  static std::string Generate(size_t size)
  {
    std::cout << "generating random reference data" << std::endl;

    auto bytes = std::string(size, '\0');

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(
          std::numeric_limits<char>::min(),
          std::numeric_limits<char>::max());

    std::generate(
          std::begin(bytes),
          std::end(bytes),
          [&]() -> char { return static_cast<char>(distribution(generator)); });

    return bytes;
  }

  static BufferPtr ToBufferPtr(char const *data, size_t size)
  {
    static BufferPool pool;

    auto ptr = pool.Get();
    ptr->assign(data, data + size);
    return ptr;
  }

  template<typename SendFn>
  void DoSendUdp(SendFn sendFn) const
  {
    // send in fixed packet sizes
    size_t pos = 0;
    while(pos + udpPacketSize < referenceData.size()) {
      pos += sendFn(referenceData.data() + pos, udpPacketSize);

      // give the receiver some time to process
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // send the remaining data not filling a whole packet
    sendFn(referenceData.data() + pos,
           referenceData.size() - pos);
  }

  template<typename SendFn>
  void DoSendTcp(SendFn sendFn) const
  {
    // send in randomly sized packets
    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> distribution(
          tcpPacketSizeMin, tcpPacketSizeMax);
    auto gen = [&]() -> size_t { return distribution(generator); };

    std::string_view remaining = referenceData;
    while(!remaining.empty()) {
      auto packetSize = std::min(gen(), remaining.size());
      auto packet = std::string_view(remaining.data(), packetSize);

      while(!packet.empty()) {
        auto sent = sendFn(packet.data(), packet.size());
        assert(sent <= packet.size());
        packet.remove_prefix(sent);
      }

      remaining.remove_prefix(packetSize);
    }
  }

  inline void Send(SocketUdpBuffered &buff,
                   Address const &dstAddr,
                   Duration perPacketTimeout) const
  {
    std::cout << "sending reference data from " << to_string(buff.LocalAddress())
              << " to " << to_string(dstAddr) << std::endl;

    auto send = [&](char const *data, size_t size) -> size_t {
      return buff.SendTo(data, size, dstAddr, perPacketTimeout);
    };
    DoSendUdp(send);
  }

  inline void Send(SocketTcpBuffered &buff,
                   Duration perPacketTimeout) const
  {
    std::cout << "sending reference data from "
              << to_string(buff.LocalAddress())
              << " to " << to_string(buff.PeerAddress())
              << std::endl;

    auto send = [&](char const *data, size_t size) -> size_t {
      return buff.Send(data, size, perPacketTimeout);
    };
    DoSendTcp(send);
  }

  inline void Send(SocketTcpAsync &async) const
  {
    std::cout << "sending reference data from "
              << to_string(async.LocalAddress())
              << " to " << to_string(async.PeerAddress())
              << std::endl;

    std::vector<std::future<void>> futures;
    futures.reserve(referenceData.size() / tcpPacketSizeMin);

    auto send = [&](char const *data, size_t size) -> size_t {
      futures.push_back(
            async.Send(ToBufferPtr(data, size)));
      return size;
    };
    DoSendTcp(send);

    // wait until everything is sent and
    // throw exception if there was any
    for(auto &&f : futures) {
      f.get();
    }
  }

  inline bool Verify(std::vector<BufferPtr> const &storage) const
  {
    std::cout << "verifying received against reference data" << std::endl;

    size_t pos = 0;
    for(auto &&packet : storage) {
      if(std::equal(
           std::begin(*packet),
           std::end(*packet),
           referenceData.data() + pos,
           referenceData.data() + pos + packet->size())) {
        pos += packet->size();
      } else {
        std::cerr << "error at byte " << pos << std::endl;
        return false;
      }
    }

    if(pos != referenceData.size()) {
      std::cerr << "received only "
                << pos
                << " of "
                << referenceData.size()
                << " bytes"
                << std::endl;
      return false;
    }

    return true;
  }
};

} // namespace sockpuppet
