#ifndef RTYPE_CLIENT_NETSTATS_HPP
#define RTYPE_CLIENT_NETSTATS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype::client {
  enum class RejectReason : std::uint8_t {
    WrongSender,
    Malformed,
    Duplicate,
    Stale,
    OutsideRoom,
    Count
  };

  struct NetStats {
    std::uint64_t packetsSent{0};
    std::uint64_t packetsReceived{0};
    std::uint64_t bytesSent{0};
    std::uint64_t bytesReceived{0};
    std::array<std::uint64_t, static_cast<std::size_t>(RejectReason::Count)> rejected{};

    double bytesSentPerSecond{0.0};
    double bytesReceivedPerSecond{0.0};

    std::optional<double> lastRttMs;
    std::optional<double> rttMs;
    std::optional<double> jitterMs;
    std::uint64_t pingsSent{0};
    std::uint64_t pongsReceived{0};
    std::uint64_t pingsLost{0};
    std::optional<double> lossPercent;

    std::uint64_t retransmissionsSent{0};
    std::uint64_t retransmissionsReceived{0};

    [[nodiscard]] std::uint64_t rejectedFor(RejectReason reason) const {
      return rejected[static_cast<std::size_t>(reason)];
    }

    [[nodiscard]] std::uint64_t rejectedTotal() const {
      std::uint64_t total = 0;
      for (const auto count : rejected) total += count;
      return total;
    }

    void reject(RejectReason reason) { ++rejected[static_cast<std::size_t>(reason)]; }
  };
}

#endif // !RTYPE_CLIENT_NETSTATS_HPP
