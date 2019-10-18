#ifdef _WIN32

#include "socket_guard.h"
#include "util.h" // for LastError

#include <winsock2.h> // for WSAStartup

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
  updateInstanceCount(1);
}

SocketGuard::~SocketGuard()
{
  updateInstanceCount(-1);
}

} // namespace sockpuppet

#endif // _WIN32
