#ifdef _WIN32
# include <windows.h> // for GetConsoleScreenBufferInfo
#endif // _WIN32

#include <iostream> // for std::cin
#include <string> // for std::string
#include <vector> // for std::vector

namespace io_print_detail {

static bool const isAnsiCapable = []() -> bool {
#ifdef _WIN32
# ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#  define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
# endif // ENABLE_VIRTUAL_TERMINAL_PROCESSING

  // TODO fix false negative for git for Windows' Bash
  auto console = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD flags = 0U;
  (void)GetConsoleMode(console, &flags);
  return (flags & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else // _WIN32
  return true;
#endif // _WIN32
}();

struct ClearLine
{
};

inline std::ostream &operator<<(std::ostream &os, ClearLine)
{
  if(isAnsiCapable) {
    os << "\x1B[2K" // clear whole current line
       << std::flush;
  } else {
#ifdef _WIN32
    auto console = GetStdHandle(STD_OUTPUT_HANDLE);

    // get line start coordinates and width
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    (void)GetConsoleScreenBufferInfo(console, &sbi);
    COORD pos{0, sbi.dwCursorPosition.Y};
    auto width = static_cast<DWORD>(sbi.dwSize.X);

    // clear with space character
    DWORD written;
    (void)FillConsoleOutputCharacter(console, ' ', width, pos, &written);
    (void)FillConsoleOutputAttribute(console, sbi.wAttributes, width, pos, &written);
#endif // _WIN32
  }
  return os;
}

static ClearLine const clearLine;


struct CursorUpFront
{
};

inline std::ostream &operator<<(std::ostream &os, CursorUpFront)
{
  if(isAnsiCapable) {
    os << "\x1B[1F" // move cursor to front 1 line up
       << std::flush;
  } else {
#ifdef _WIN32
    auto console = GetStdHandle(STD_OUTPUT_HANDLE);

    // get coordinates
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    (void)GetConsoleScreenBufferInfo(console, &sbi);

    // position cursor
    COORD pos{0, static_cast<SHORT>(sbi.dwCursorPosition.Y - 1)};
    (void)SetConsoleCursorPosition(console, pos);
#endif // _WIN32
  }
  return os;
}

static CursorUpFront const cursorUpFront;


/// Moves cursor to front N lines up on instantiation and
/// resets it on release. In the meantime lines can be
/// printed to normally (advance using regular newlines).
/// @note  It is not possible to insert new lines this way.
/// @note  Line contents are not modified.
struct CursorUpFrontGuard
{
  std::ostream &os;
#ifdef _WIN32
  HANDLE console;
  COORD inputPos;
#endif // _WIN32

  CursorUpFrontGuard(std::ostream &os, size_t count)
    : os(os)
  {
    if(isAnsiCapable) {
      os << "\x1B[s" // save current cursor position
         << "\x1B[" << count << "F" // move cursor to front N lines up
         << std::flush;
    } else {
#ifdef _WIN32
      console = GetStdHandle(STD_OUTPUT_HANDLE);

      // get coordinates
      CONSOLE_SCREEN_BUFFER_INFO sbi;
      (void)GetConsoleScreenBufferInfo(console, &sbi);
      inputPos = sbi.dwCursorPosition;
      COORD outputPos{0, static_cast<SHORT>(inputPos.Y - static_cast<SHORT>(count))};

      // position cursor
      (void)SetConsoleCursorPosition(console, outputPos);
#endif // _WIN32
    }
  }

  ~CursorUpFrontGuard()
  {
    if(isAnsiCapable) {
      os << "\x1B[u" // restore saved cursor position
         << std::flush;
    } else {
#ifdef _WIN32
      (void)SetConsoleCursorPosition(console, inputPos);
#endif // _WIN32
    }
  }
};

} // namespace io_print_detail

/// Manages a given number of terminal lines to print a history
/// of messages (e.g. external input) while user input can be entered below
/// without being interrupted by asynchronous message prints.
/// The buffer keeps the number of terminal lines constant and erases
/// its own history in FIFO fashion.
/// @note  Messages longer than a terminal line cause undefined behaviour.
struct IOPrintBuffer : private std::vector<std::string>
{
  std::ostream &os;

  IOPrintBuffer(std::ostream &os, size_t length)
    : std::vector<std::string>(length)
    , os(os)
  {
    for(size_t i = 0U; i <= length; ++i) {
      os << '\n';
    }
    os << io_print_detail::cursorUpFront
       << io_print_detail::clearLine
       << std::flush;
  }

  inline std::string Query(std::string const &prompt) const
  {
    os << prompt << std::flush;

    std::string line;
    (void)std::getline(std::cin, line);

    os << io_print_detail::cursorUpFront
       << io_print_detail::clearLine
       << std::flush;

    return line;
  }

  inline void Print(std::string message)
  {
    this->erase(this->begin());
    this->push_back(std::move(message));

    Print();
  }

  inline void Print()
  {
    io_print_detail::CursorUpFrontGuard ufg(os, this->size());
    for(auto &&line : *this) {
      os << io_print_detail::clearLine << line << std::endl;
    }
  }
};
