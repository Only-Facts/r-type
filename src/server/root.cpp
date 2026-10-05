#include "cli.hpp"
#include "exception.hpp"
#include "variables.hpp"
#include "network/SFMLUdpSocket.hpp"
#include <cstddef>
#include <string>

#define DEFAULT_PORT 4242

std::size_t parse_arguments(int argc, const char *argv[], std::size_t* port) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
    else if (flag == "--port" || flag == "-p") {
      if (!argv[i + 1]) return bad_usage();
      std::size_t p = std::stoul(argv[++i]);
      *port = p;
    } else return bad_usage();
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  std::size_t port = DEFAULT_PORT;
  std::size_t bits = parse_arguments(argc, argv, &port);

  if (bits & (FAIL | ERROR | HELP)) [[ unlikely ]] { return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS; }

  try {
    rtype::network::SFMLUdpSocket serverSocket;
    serverSocket.bind(port);
    serverSocket.setNonBlocking(true);

    info(("Server listening on port " + std::string(BOLD) + std::string(ITALIC) + std::string(BLUE) + std::to_string(serverSocket.getLocalPort())).c_str());

    bool isRunning = true;
    rtype::network::PacketData incomingData;
    rtype::network::Endpoint sender;

    while (isRunning) {
      while (serverSocket.receive(incomingData, sender)) {
        std::string msg = "Received " + std::to_string(incomingData.size()) +
          " bytes from " + sender.address + ":" + std::to_string(sender.port);

        info(msg.c_str());
      }
    }
    return SUCCESS;
  } catch (const rtype::RtypeError& e) {
    error(e.what());
    return ERROR;
  } catch (const std::exception& e) {
    error(("Uncaught error: " + std::string(e.what())).c_str());
    return FAIL;
  } catch (...) {
    error("Uncaught error.");
    return FAIL;
  }
}
