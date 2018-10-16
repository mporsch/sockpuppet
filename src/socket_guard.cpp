#include "socket_guard.h"

#ifdef _WIN32
# include <winsock2.h> // for WSAStartup
# pragma comment(lib, "wsock32.lib")
#endif // _WIN32

#include <stdexcept> // for std::runtime_error
#include <string> // for std::to_string

unsigned int SocketGuard::m_instanceCount{};
std::mutex SocketGuard::m_mtx{};

SocketGuard::SocketGuard()
{
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(m_mtx);

  if(m_instanceCount == 0U) {
    // we are the first instance -> initialize
    WSADATA wsaData;
    if(auto const result = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
      throw std::runtime_error("failed to intitialize socket subsystem: "
                               + std::to_string(result));
    }
  }
  ++m_instanceCount;
#endif // _WIN32
}

SocketGuard::~SocketGuard()
{
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(m_mtx);

  --m_instanceCount;
  if(m_instanceCount == 0U) {
    // we are the last instance -> cleanup
    (void)WSACleanup();
  }
#endif // _WIN32
}
