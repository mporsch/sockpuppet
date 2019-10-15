#include "socket_guard.h"

#ifdef _WIN32
# include <Winsock2.h> // for WSAStartup
#endif // _WIN32

#include <mutex> // for std::mutex
#include <stdexcept> // for std::runtime_error
#include <string> // for std::to_string

namespace sockpuppet {

namespace {
  void updateInstanceCount(int modifier)
  {
    static std::mutex mtx;
    static int curr = 0;

    std::lock_guard<std::mutex> lock(mtx);

    auto const prev = curr;
    curr += modifier;

    if(prev == 0 && curr == 1) {
      // we are the first instance -> initialize
      WSADATA wsaData;
      if(auto const result = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw std::runtime_error("failed to intitialize socket subsystem: "
                                 + std::to_string(result));
      }
    } else if(prev == 1 && curr == 0) {
      // we are the last instance -> cleanup
      (void)WSACleanup();
    }
  }
} // unnamed namespace

SocketGuard::SocketGuard()
{
#ifdef _WIN32
  updateInstanceCount(1);
#endif // _WIN32
}

SocketGuard::~SocketGuard()
{
#ifdef _WIN32
  updateInstanceCount(-1);
#endif // _WIN32
}

} // namespace sockpuppet
