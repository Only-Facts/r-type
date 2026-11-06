#include "cli.hpp"
#include "exception.hpp"
#include "variables.hpp"
#include "network/SFMLUdpSocket.hpp"
#include "ServerNetworkManager.hpp"
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#define DEFAULT_PORT 4242
#define DEFAULT_MAX_ROOMS 16
#define DEFAULT_ROOM_CAPACITY 4

std::size_t parse_arguments(int argc, const char *argv[],
                            std::size_t* port, std::size_t* maxRooms,
                            std::size_t* roomCapacity) {
  for (std::size_t i = 1; i < static_cast<std::size_t>(argc); ++i) {
    const std::string flag(argv[i]);
    if (flag == "--help" || flag == "-h") return print_usage();
    if (i + 1 >= static_cast<std::size_t>(argc)) return bad_usage();
    try {
      const std::string argument(argv[++i]);
      std::size_t parsed = 0;
      const auto value = std::stoul(argument, &parsed);
      if (parsed != argument.size()) return bad_usage();
      if (flag == "--port" || flag == "-p") {
        if (value == 0 || value > 65535) return bad_usage();
        *port = value;
      } else if (flag == "--max-rooms") {
        if (value == 0 || value > 128) return bad_usage();
        *maxRooms = value;
      } else if (flag == "--room-capacity") {
        if (value == 0 || value > 4) return bad_usage();
        *roomCapacity = value;
      } else {
        return bad_usage();
      }
    } catch (const std::exception&) {
      return bad_usage();
    }
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  std::size_t port = DEFAULT_PORT;
  std::size_t maxRooms = DEFAULT_MAX_ROOMS;
  std::size_t roomCapacity = DEFAULT_ROOM_CAPACITY;
  std::size_t bits = parse_arguments(argc, argv, &port, &maxRooms,
                                     &roomCapacity);

  if (bits & (FAIL | ERROR | HELP)) [[ unlikely ]] { return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS; }

  try {
    rtype::network::SFMLUdpSocket serverSocket;
    serverSocket.bind(static_cast<unsigned short>(port));
    serverSocket.setNonBlocking(true);

    const std::string message = "Server listening on port " +
      std::string(BOLD) + std::string(ITALIC) + std::string(BLUE) +
      std::to_string(serverSocket.getLocalPort());
    info(message.c_str());

    rtype::server::ServerNetworkManager serverNet(
      serverSocket, maxRooms, roomCapacity);
    serverNet.startNetworkThread();
    info("Multi-instance server ready.");
    auto lastTime = std::chrono::steady_clock::now();
    auto nextFrame = lastTime;
    auto nextStats = lastTime + std::chrono::seconds(10);
    auto previousStats = serverNet.stats();
    constexpr auto frameDuration = std::chrono::microseconds(16667);

    while (true) {
      nextFrame += frameDuration;
      const auto now = std::chrono::steady_clock::now();
      const float dt = std::chrono::duration<float>(now - lastTime).count();
      lastTime = now;
      serverNet.update(dt);

      if (now >= nextStats) {
        const auto current = serverNet.stats();
        const double seconds = 10.0;
        const double rxKiB = static_cast<double>(
          current.receivedBytes - previousStats.receivedBytes) /
          seconds / 1024.0;
        const double txKiB = static_cast<double>(
          current.sentBytes - previousStats.sentBytes) /
          seconds / 1024.0;
        std::ostringstream report;
        report << "Network stats: rooms=" << current.rooms
               << " players=" << current.players
               << " rx=" << std::fixed << std::setprecision(1) << rxKiB
               << " KiB/s tx=" << txKiB << " KiB/s"
               << " reliable(pending=" << current.reliablePending
               << ", acked=" << current.reliableAcknowledged
               << ", retried=" << current.reliableRetransmitted
               << ", timeout=" << current.reliableTimedOut << ')'
               << " malformed=" << current.malformedPackets
               << " rejected=" << current.rejectedPackets
               << " rate-limited=" << current.rateLimitedPackets
               << " queue-drops=" << current.receiveQueueDrops;
        info(report.str().c_str());
        previousStats = current;
        nextStats = now + std::chrono::seconds(10);
      }

      std::this_thread::sleep_until(nextFrame);
      const auto afterSleep = std::chrono::steady_clock::now();
      if (afterSleep > nextFrame + std::chrono::milliseconds(100))
        nextFrame = afterSleep;
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
