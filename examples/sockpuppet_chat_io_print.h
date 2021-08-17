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
#else // _WIN32 // _WIN32
  return true;
#endif // _WIN32 // _WIN32
}() || true;

struct ClearLine
{
};

inline std::ostream &operator<<(std::ostream &os, ClearLine)
{
  if(isAnsiCapable) {
    os << '\r' // move cursor to the left
       << "\x1B[K" // clear everything right of cursor
       << std::flush;
  } else {
#ifdef _WIN32
    auto console = GetStdHandle(STD_OUTPUT_HANDLE);

    // get coordinates
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    (void)GetConsoleScreenBufferInfo(console, &sbi);
    COORD coord = sbi.dwCursorPosition;
    auto width = static_cast<DWORD>(sbi.dwSize.X);

    DWORD written;
    (void)FillConsoleOutputCharacter(console, ' ', width, coord, &written);
    (void)FillConsoleOutputAttribute(console, sbi.wAttributes, width, coord, &written);
#endif // _WIN32
  }
  return os;
}

static ClearLine const clearLine;


struct CursorUp
{
};

inline std::ostream &operator<<(std::ostream &os, CursorUp)
{
  if(isAnsiCapable) {
    os << "\x1B[1F" // move cursor up front
       << "\x1B[K" // clear everything right of cursor
       << std::flush;
  } else {
#ifdef _WIN32
    auto console = GetStdHandle(STD_OUTPUT_HANDLE);

    // get coordinates
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    (void)GetConsoleScreenBufferInfo(console, &sbi);
    COORD coord{0, static_cast<SHORT>(sbi.dwCursorPosition.Y - 1)};
    auto width = static_cast<DWORD>(sbi.dwSize.X);

    // clear line
    DWORD written;
    (void)FillConsoleOutputCharacter(console, ' ', width, coord, &written);
    (void)FillConsoleOutputAttribute(console, sbi.wAttributes, width, coord, &written);

    // position cursor
    (void)SetConsoleCursorPosition(console, coord);
#endif // _WIN32
  }
  return os;
}

static CursorUp const cursorUp;

} // namespace io_print_detail

struct IOPrintBuffer : private std::vector<std::string>
{
  std::ostream &os;

  struct CursorUpGuard
  {
    std::ostream &os;
#ifdef _WIN32
    HANDLE console;
    COORD inputCoord;
#endif // _WIN32

    CursorUpGuard(std::ostream &os, size_t length)
      : os(os)
    {
      if(io_print_detail::isAnsiCapable) {
        os << "\x1B[s" // save current cursor position
           << "\x1B[" << length << "F" // move cursor up front
           << std::flush;
      } else {
#ifdef _WIN32
        console = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO sbi;
        (void)GetConsoleScreenBufferInfo(console, &sbi);

        inputCoord = sbi.dwCursorPosition;

        COORD outputCoord{0, static_cast<SHORT>(inputCoord.Y - length)};
        (void)SetConsoleCursorPosition(console, outputCoord);
#endif // _WIN32
      }
    }

    ~CursorUpGuard()
    {
      if(io_print_detail::isAnsiCapable) {
        os << "\x1B[u" << std::flush; // restore saved cursor position
      } else {
#ifdef _WIN32
        (void)SetConsoleCursorPosition(console, inputCoord);
#endif // _WIN32
      }
    }
  };

  IOPrintBuffer(std::ostream &os, size_t length)
    : std::vector<std::string>(length)
    , os(os)
  {
    for(size_t i = 0U; i < length + 1; ++i) {
      os << '\n';
    }
    os << io_print_detail::cursorUp << std::flush;
  }

  inline std::string Query(std::string const &prompt) const
  {
    os << prompt << std::flush;

    std::string line;
    std::getline(std::cin, line);

    os << io_print_detail::cursorUp << std::flush;

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
    CursorUpGuard sbg(os, this->size());
    for(auto &&line : *this) {
      os << io_print_detail::clearLine << line << std::endl;
    }
  }
};
