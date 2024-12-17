#ifdef _WIN32

#include "winsock_guard.h"
#include "error_code.h" // for SocketError

#include <winsock2.h> // for WSAStartup

#include <atomic> // for std::atomic
#include <mutex> // for std::mutex

namespace sockpuppet {

namespace {

std::atomic<int> &Count()
{
  static std::atomic<int> curr = 0;
  return curr;
}

void Dec() noexcept
{
  if(Count()-- == 1) {
    // we were the last instance -> cleanup
    (void)::WSACleanup();
  }
}

void Inc()
{
  static std::mutex mtx;
  std::lock_guard<std::mutex> lock(mtx);

  if(Count()++ == 0) {
    // we are the first instance -> initialize
    WSADATA wsaData;
    if(auto result = ::WSAStartup(MAKEWORD(2, 2), &wsaData)) {
      throw std::system_error(SocketError(result), "failed to intitialize socket subsystem");
    }
  }
}

} // unnamed namespace

WinSockGuard::WinSockGuard()
{
  Inc();
}

WinSockGuard::~WinSockGuard() noexcept
{
  Dec();
}

} // namespace sockpuppet

#endif // _WIN32
