#ifdef _WIN32

#include "winsock_guard.h"
#include "error_code.h" // for SocketError

#include <winsock2.h> // for WSAStartup

#include <mutex> // for std::mutex

namespace sockpuppet {

namespace {
  void UpdateInstanceCount(int modifier)
  {
    static std::mutex mtx;
    static int curr = 0;

    std::lock_guard<std::mutex> lock(mtx);

    auto const prev = curr;
    curr += modifier;

    if(prev == 0 && curr == 1) {
      // we are the first instance -> initialize
      WSADATA wsaData;
      if(auto const result = ::WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw std::system_error(SocketError(result), "failed to intitialize socket subsystem");
      }
    } else if(prev == 1 && curr == 0) {
      // we are the last instance -> cleanup
      (void)::WSACleanup();
    }
  }
} // unnamed namespace

WinSockGuard::WinSockGuard()
{
  UpdateInstanceCount(1);
}

WinSockGuard::~WinSockGuard()
{
  UpdateInstanceCount(-1);
}

} // namespace sockpuppet

#endif // _WIN32
