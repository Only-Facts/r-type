#ifndef SERVERMETRICS_HPP
#define SERVERMETRICS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace rtype::server {
  struct ServerMetrics {
    std::atomic<std::uint64_t> receivedDatagrams{0};
    std::atomic<std::uint64_t> receivedBytes{0};
    std::atomic<std::uint64_t> sentDatagrams{0};
    std::atomic<std::uint64_t> sentBytes{0};
    std::atomic<std::uint64_t> malformedPackets{0};
    std::atomic<std::uint64_t> rejectedPackets{0};
    std::atomic<std::uint64_t> rateLimitedPackets{0};
    std::atomic<std::uint64_t> receiveQueueDrops{0};
    std::atomic<std::uint64_t> reliableQueueDrops{0};
    std::atomic<double> droppedSimulationSeconds{0.0};
    std::atomic<std::uint64_t> fullSnapshotsSent{0};
    std::atomic<std::uint64_t> fullSnapshotBytes{0};
    std::atomic<std::uint64_t> deltaSnapshotsSent{0};
    std::atomic<std::uint64_t> deltaSnapshotBytes{0};
    std::atomic<std::uint64_t> fullSnapshotRequests{0};

    void recordReceive(std::size_t bytes) {
      receivedDatagrams.fetch_add(1, std::memory_order_relaxed);
      receivedBytes.fetch_add(bytes, std::memory_order_relaxed);
    }

    void recordSend(std::size_t bytes) {
      sentDatagrams.fetch_add(1, std::memory_order_relaxed);
      sentBytes.fetch_add(bytes, std::memory_order_relaxed);
    }

    void recordSnapshot(bool full, std::size_t bytes) {
      if (full) {
        fullSnapshotsSent.fetch_add(1, std::memory_order_relaxed);
        fullSnapshotBytes.fetch_add(bytes, std::memory_order_relaxed);
      } else {
        deltaSnapshotsSent.fetch_add(1, std::memory_order_relaxed);
        deltaSnapshotBytes.fetch_add(bytes, std::memory_order_relaxed);
      }
    }
  };

  struct ServerStats {
    std::uint64_t receivedDatagrams{0};
    std::uint64_t receivedBytes{0};
    std::uint64_t sentDatagrams{0};
    std::uint64_t sentBytes{0};
    std::uint64_t malformedPackets{0};
    std::uint64_t rejectedPackets{0};
    std::uint64_t rateLimitedPackets{0};
    std::uint64_t receiveQueueDrops{0};
    std::uint64_t reliableQueueDrops{0};
    std::uint64_t reliablePending{0};
    std::uint64_t reliableAcknowledged{0};
    std::uint64_t reliableRetransmitted{0};
    std::uint64_t reliableTimedOut{0};
    std::size_t rooms{0};
    std::size_t players{0};
    double droppedSimulationSeconds{0.0};
    std::uint64_t fullSnapshotsSent{0};
    std::uint64_t fullSnapshotBytes{0};
    std::uint64_t deltaSnapshotsSent{0};
    std::uint64_t deltaSnapshotBytes{0};
    std::uint64_t fullSnapshotRequests{0};
  };
}

#endif // !SERVERMETRICS_HPP
