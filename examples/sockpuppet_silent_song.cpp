#include "sockpuppet/socket_async.h" // for Driver

#include <csignal> // for std::signal
#include <cstdlib> // for EXIT_SUCCESS
#include <fstream> // for std::ifstream
#include <iomanip> // for std::setw
#include <iostream> // for std::cout
#include <regex> // for std::regex
#include <sstream> // for std::stringstream
#include <string> // for std::string

using namespace sockpuppet;

namespace {

Duration ParseMinutes(std::string str)
{
  using namespace std::chrono;

  minutes::rep min;
  if(!(std::stringstream(str) >> min))
    throw std::invalid_argument("invalid minutes");

  return minutes(min);
}

Duration ParseSeconds(std::string str)
{
  using namespace std::chrono;
  using Seconds = duration<float>;

  Seconds::rep sec;
  if(!(std::stringstream(str) >> sec))
    throw std::invalid_argument("invalid seconds");

  return duration_cast<Duration>(Seconds(sec));
}

std::string PutTime(Duration time)
{
  using namespace std::chrono;
  using centiseconds = duration<milliseconds::rep, std::centi>;

  auto min = duration_cast<minutes>(time);
  auto sec = duration_cast<seconds>(time - min);
  auto csec = duration_cast<centiseconds>(time - min - sec);

  std::stringstream ss;
  ss << std::setw(2) << std::setfill('0')
     << min.count()
     << ":"
     << std::setw(2) << std::setfill('0')
     << sec.count()
     << "."
     << std::setw(2) << std::setfill('0')
     << csec.count();

  return ss.str();
}

void Sing(Duration time, std::string text)
{
  std::cout << "[" << PutTime(time) << "] "
            << text
            << std::endl;
}

void ParseAndSchedule(Driver &driver, std::ifstream ifs)
{
  std::regex const lrcLineRegex(R"(\[([0-9]+):([0-9.]+)\](.*))");

  // schedule a task for stopping the driver loop
  // the task will be shifted back according to the LRC timings
  auto fin = [&driver]() {
    std::cout << "~~FIN~~" << std::endl;
    driver.Stop();
  };
  ToDo finale(driver, fin, Duration(0));

  std::string line;
  while(std::getline(ifs, line)) {
    // find and parse text lines using regex
    std::smatch match;
    if(std::regex_match(line, match, lrcLineRegex)) {
      auto minutes = match[1].str();
      auto seconds = match[2].str();
      auto text = match[3].str();

      // parse line time
      auto time = ParseMinutes(minutes) + ParseSeconds(seconds);

      // schedule line print
      // no need to keep the created object
      // as we don't intend to shift or cancel it
      (void)ToDo(driver, std::bind(Sing, time, text), time);

      // delay the shutdown task to after the last line print
      finale.Shift(time);
    }
  }
}

void Run(std::ifstream ifs)
{
  // socket driver to handle timing
  static Driver driver;

  // set up the handler for Ctrl-C
  auto signalHandler = [](int) {
    driver.Stop();
  };
  if(std::signal(SIGINT, signalHandler) == SIG_ERR) {
    throw std::logic_error("failed to set signal handler");
  }

  // read LRC lines and schedule printouts
  ParseAndSchedule(driver, std::move(ifs));

  // start driver/timer loop
  driver.Run();
}

} // unnamed namespace

int main(int argc, char **argv)
try {
  if(argc == 2) {
    std::ifstream ifs(argv[1]);
    if(ifs.is_open()) {
      Run(std::move(ifs));
      return EXIT_SUCCESS;
    } else {
      std::cerr << "File not found \"" << argv[1] << "\"\n\n";
    }
  }

  std::cout << "Example application for sockpuppet ToDos with\n"
       "the totally network-unrelated use case of displaying\n"
       "lyrics with their timings as given in an LRC file\n\n"
       "Usage: "
    << argv[0] << " LRC\n\n"
       "\tLRC is a path to an LRC (lyrics text) file to play back,\n"
       "\te.g. as found at\n"
       "\thttps://www.megalobiz.com/lrc/maker/Bohemian+Rhapsody.54490345"
    << std::endl;

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
