#include "socket_guard.h"
#include "util.h" // for LastError

#ifdef _WIN32
# include <Winsock2.h> // for WSAStartup
#endif // _WIN32

#include <mutex> // for std::mutex
#include <system_error> // for std::system_error

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
        throw std::system_error(LastError(), "failed to intitialize socket subsystem");
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
