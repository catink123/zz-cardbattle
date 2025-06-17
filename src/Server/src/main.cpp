#include <boost/asio/ip/address.hpp>
#include <iostream>
#include <thread>
#include "ConfigurableServer.h"

const int THREADS = 4;

int main(int argc, char* argv[])
{
  if (argc != 5)
  {
    std::cerr <<
      "Usage: " << argv[0] << " <address> <port> <doc_root>" << std::endl;
  }
  auto const address = net::ip::make_address(argv[1]);
  auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
  auto const docRoot = std::make_shared<std::string>(argv[3]);

  net::io_context ioc{THREADS};

  std::make_shared<ConfigurableServer>
  (
    ioc,
    tcp::endpoint{address, port},
    docRoot
  )->Run();

  std::vector<std::thread> threads;
  threads.reserve(THREADS - 1);
  for (auto i = THREADS - 1; i > 0; --i)
  {
    threads.emplace_back
    (
      [&ioc]
      {
        ioc.run();
      }
    );
  }
  
  return 0;
}
