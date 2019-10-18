#ifdef _WIN32

#include "util.h"

#include <winsock2.h> // for WSAGetLastError

// Win32/Winsock error code handling inspired by
// https://gist.github.com/bbolli/710010adb309d5063111889530237d6d

namespace {

/// Wrap the winsock error code so we have a distinct type.
struct winsock_error_code
{
  int error;

  winsock_error_code(int e) noexcept
    : error(e)
  {
  }
};

namespace detail {

/// The Win32 error code category.
struct win32_error_category : public std::error_category
{
  /// Return a short descriptive name for the category.
  char const *name() const noexcept override
  {
    return "Win32Error";
  }

  /// Return what each error code means in text.
  std::string message(int messageId) const override
  {
    char buffer[256];
    if(DWORD const len = FormatMessage(
         FORMAT_MESSAGE_FROM_SYSTEM,
         nullptr,
         static_cast<DWORD>(messageId),
         0U,
         buffer, sizeof(buffer),
         nullptr)) {
      std::string message(buffer, len);

      // trim trailing newline
      while(!message.empty() &&
            ((message.back() == '\r') || (message.back() == '\n'))) {
        message.pop_back();
      }

      return message;
    } else {
      return "Unknown error";
    }
  }
};

} // namespace detail

/// Return a static instance of the custom category.
detail::win32_error_category const &win32_error_category()
{
  static detail::win32_error_category const c;
  return c;
}

// Overload the global make_error_code() free function with our
// custom error. It will be found via ADL by the compiler if needed.
std::error_code make_error_code(winsock_error_code const &we)
{
  return std::error_code(we.error, win32_error_category());
}

} // unnamed namespace

namespace std {

// Tell the C++ 11 STL metaprogramming that winsock_error_code
// is registered with the standard error code system.
template<>
struct is_error_code_enum<winsock_error_code> : std::true_type
{
};

} // namespace std

namespace sockpuppet {

std::error_code LastError()
{
  return make_error_code(winsock_error_code(WSAGetLastError()));
}

} // namespace sockpuppet

#endif // _WIN32
