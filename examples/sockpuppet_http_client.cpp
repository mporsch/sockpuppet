#include "sockpuppet/socket_buffered.h" // for SocketTcpBuffered

#include <cstdlib> // for EXIT_SUCCESS
#include <functional> // for std::plus
#include <iostream> // for std::cout
#include <numeric> // for std::accumulate
#include <stdexcept> // for std::exception
#include <vector> // for std::vector

using namespace sockpuppet;

namespace {
std::string HttpGet(std::string const &host, std::string const &path)
{
  return "GET " + path + " HTTP/1.1\nHost: " + host + "\n\n";
}

bool IsHtmlEnd(std::string const &response)
{
  return (response.find("</html>") != std::string::npos);
}
} // unnamed namespace

int main(int, char **)
try {
  std::string serv = "http://";
  std::string host = "www.google.com";
  std::string path = "/";

  // connect to server
  Address addr(serv + host);
  SocketTcpBuffered buff(addr);

  // send HTTP GET request
  auto request = HttpGet(host, path);
  (void)buff.Send(request.c_str(), request.size());

  std::vector<size_t> sizes;
  for(;;) {
    // receive response (fragment)
    auto response = buff.Receive(std::chrono::seconds(30));

    // break on timeout
    if(response->empty()) {
      break;
    }

    // print and collect statistics
    std::cout << *response;
    sizes.push_back(response->size());

    // early break on response end
    if(IsHtmlEnd(*response)) {
      break;
    }
  }

  // print statistics
  std::cout << "\n\nreceived "
            << std::accumulate(std::begin(sizes), std::end(sizes),
                               size_t(0U), std::plus<size_t>())
            << " bytes in "
            << sizes.size()
            << " buffers"
            << std::endl;

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
} catch (...) {
  return EXIT_FAILURE;
}
