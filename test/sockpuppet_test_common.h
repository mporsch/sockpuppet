#include "sockpuppet/socket_async.h" // for SocketTcpAsyncClient

#include <algorithm> // for std::generate
#include <iostream> // for std::cout
#include <random> // for std::default_random_engine
#include <thread> // for std::this_thread
#include <vector> // for std::vector

namespace sockpuppet {

struct TestData
{
  std::vector<char> const referenceData;

  TestData(size_t size)
    : referenceData(Generate(size))
  {
  }

  static std::vector<char> Generate(size_t size)
  {
    std::cout << "generating random reference data" << std::endl;

    std::vector<char> bytes(size);

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

  static SocketAsync::SocketBufferPtr ToBufferPtr(char const *data, size_t size)
  {
    static SocketAsync::SocketBufferPool pool;

    auto ptr = pool.Get();
    ptr->assign(data, data + size);
    return ptr;
  }

  template<typename SendFn>
  void DoSendUdp(SendFn sendFn) const
  {
    // send in fixed packet sizes
    size_t pos = 0;
    static size_t const packetSize = 1400U;
    for(; pos + packetSize < referenceData.size(); pos += packetSize) {
      sendFn(referenceData.data() + pos, packetSize);

      // delay packets to avoid loss / out-of-order / queue overflow
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
    std::uniform_int_distribution<size_t> distribution(100U, 10000U);
    auto gen = [&]() -> size_t { return distribution(generator); };

    size_t pos = 0;
    auto packetSize = gen();
    while(pos + packetSize < referenceData.size()) {
      sendFn(referenceData.data() + pos, packetSize);
      pos += packetSize;
      packetSize = gen();
    }

    // send the remaining data not filling a whole packet
    sendFn(referenceData.data() + pos,
           referenceData.size() - pos);
  }

  inline void Send(SocketUdpBuffered &buff, Address const &dstAddr) const
  {
    std::cout << "sending reference data from " << to_string(buff.LocalAddress())
              << " to " << to_string(dstAddr) << std::endl;

    auto send = [&](char const *data, size_t size) {
      (void)buff.SendTo(data, size, dstAddr);
    };
    DoSendUdp(send);
  }

  inline void Send(SocketTcpBuffered &buff) const
  {
    std::cout << "sending reference data from "
              << to_string(buff.LocalAddress())
              << " to " << to_string(buff.PeerAddress())
              << std::endl;

    auto send = [&](char const *data, size_t size) {
      (void)buff.Send(data, size);
    };
    DoSendTcp(send);
  }

  inline void Send(SocketTcpAsyncClient &async) const
  {
    std::cout << "sending reference data from "
              << to_string(async.LocalAddress())
              << " to " << to_string(async.PeerAddress())
              << std::endl;

    auto send = [&](char const *data, size_t size) {
      (void)async.Send(ToBufferPtr(data, size));
    };
    DoSendTcp(send);
  }

  inline bool Verify(std::vector<SocketUdpBuffered::SocketBufferPtr> const &storage) const
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
        std::cout << "error at byte " << pos << std::endl;
        return false;
      }
    }

    return true;
  }
};

} // namespace sockpuppet
