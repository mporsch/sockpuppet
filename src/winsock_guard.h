#ifndef SOCKPUPPET_WINSOCK_GUARD_H
#define SOCKPUPPET_WINSOCK_GUARD_H

namespace sockpuppet {

struct WinSockGuard
{
#ifdef _WIN32
  WinSockGuard();
  WinSockGuard(WinSockGuard const &other) = delete;
  WinSockGuard(WinSockGuard &&other) = delete;
  ~WinSockGuard() noexcept;
  WinSockGuard &operator=(WinSockGuard const &other) = delete;
  WinSockGuard &operator=(WinSockGuard &&other) = delete;
#endif // _WIN32
};

} // namespace sockpuppet

#endif // SOCKPUPPET_WINSOCK_GUARD_H
