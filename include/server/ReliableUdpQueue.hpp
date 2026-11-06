#ifndef RELIABLEUDPQUEUE_HPP
#define RELIABLEUDPQUEUE_HPP

#include "ServerMetrics.hpp"
#include "network/INetworkSocket.hpp"
#include "network/PacketStream.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtype::server {
  class ReliableUdpQueue {
  public:
    static constexpr std::size_t MAX_PENDING_PACKETS = 4096;
    static constexpr std::size_t MAX_PENDING_BYTES = 4 * 1024 * 1024;
    static constexpr std::size_t MAX_PENDING_PER_ENDPOINT = 256;
    static constexpr std::uint8_t MAX_RETRIES = 5;
    static constexpr float INITIAL_RTO = 0.15f;
    static constexpr float MAX_RTO = 1.2f;

    explicit ReliableUdpQueue(ServerMetrics* metrics = nullptr)
      : _metrics(metrics) {}

    bool send(network::INetworkSocket& socket,
              const network::PacketData& packet,
              const network::Endpoint& endpoint) {
      network::PacketHeader header{};
      if (!readAndValidateHeader(packet, header)) {
        ++_invalid;
        return false;
      }
      if (!isReliable(header.type)) {
        sendTracked(socket, packet, endpoint);
        return true;
      }

      auto existing = find(endpoint, header.sequence);
      if (existing != _pending.end()) {
        if (existing->packet != packet) {
          ++_invalid;
          return false;
        }
        sendTracked(socket, existing->packet, existing->endpoint);
        return true;
      }

      if (_pending.size() >= MAX_PENDING_PACKETS ||
          packet.size() > MAX_PENDING_BYTES - _pendingBytes ||
          pendingFor(endpoint) >= MAX_PENDING_PER_ENDPOINT) {
        ++_dropped;
        if (_metrics)
          _metrics->reliableQueueDrops.fetch_add(1, std::memory_order_relaxed);
        return false;
      }

      sendTracked(socket, packet, endpoint);
      _pending.push_back(Pending{
        packet, endpoint, header.sequence, INITIAL_RTO, INITIAL_RTO, 0
      });
      _pendingBytes += packet.size();
      return true;
    }

    bool acknowledge(const network::Endpoint& endpoint,
                     std::uint32_t sequence) {
      const auto it = find(endpoint, sequence);
      if (it == _pending.end()) return false;
      _pendingBytes -= it->packet.size();
      _pending.erase(it);
      ++_acknowledged;
      return true;
    }

    void cancelEndpoint(const network::Endpoint& endpoint) {
      for (auto it = _pending.begin(); it != _pending.end();) {
        if (sameEndpoint(it->endpoint, endpoint)) {
          _pendingBytes -= it->packet.size();
          it = _pending.erase(it);
        } else {
          ++it;
        }
      }
    }

    void update(network::INetworkSocket& socket, float dt) {
      const float elapsed = std::clamp(dt, 0.0f, 1.0f);
      for (auto it = _pending.begin(); it != _pending.end();) {
        it->remaining -= elapsed;
        if (it->remaining > 0.0f) {
          ++it;
          continue;
        }
        if (it->retries >= MAX_RETRIES) {
          _pendingBytes -= it->packet.size();
          it = _pending.erase(it);
          ++_timedOut;
          continue;
        }
        sendTracked(socket, it->packet, it->endpoint);
        ++it->retries;
        it->interval = std::min(MAX_RTO, it->interval * 2.0f);
        it->remaining = it->interval;
        ++_retransmitted;
        ++it;
      }
    }

    [[nodiscard]] std::size_t pendingCount() const {
      return _pending.size();
    }
    [[nodiscard]] std::size_t pendingBytes() const { return _pendingBytes; }
    [[nodiscard]] std::size_t droppedCount() const { return _dropped; }
    [[nodiscard]] std::size_t invalidCount() const { return _invalid; }
    [[nodiscard]] std::size_t retransmittedCount() const {
      return _retransmitted;
    }
    [[nodiscard]] std::size_t timedOutCount() const { return _timedOut; }
    [[nodiscard]] std::size_t acknowledgedCount() const {
      return _acknowledged;
    }

    static bool isReliable(network::PacketType type) {
      switch (type) {
        case network::PacketType::ConnectAck:
        case network::PacketType::ConnectReject:
        case network::PacketType::RoomListAck:
        case network::PacketType::RoomCreateAck:
        case network::PacketType::RoomJoinAck:
        case network::PacketType::RoomLeaveAck:
        case network::PacketType::SpawnPlayer:
        case network::PacketType::DestroyEntity:
        case network::PacketType::SpawnEntity:
        case network::PacketType::HealthUpdate:
        case network::PacketType::AugmentUpdate:
        case network::PacketType::ScoreUpdate:
          return true;
        default:
          return false;
      }
    }

  private:
    struct Pending {
      network::PacketData packet;
      network::Endpoint endpoint;
      std::uint32_t sequence{0};
      float interval{INITIAL_RTO};
      float remaining{INITIAL_RTO};
      std::uint8_t retries{0};
    };

    using Iterator = std::vector<Pending>::iterator;

    static bool sameEndpoint(const network::Endpoint& lhs,
                             const network::Endpoint& rhs) {
      return lhs.address == rhs.address && lhs.port == rhs.port;
    }

    static bool readAndValidateHeader(const network::PacketData& packet,
                                      network::PacketHeader& header) {
      if (packet.size() < sizeof(network::PacketHeader)) return false;
      try {
        network::PacketStreamReader reader(packet);
        reader >> header;
        return header.magic == network::PROTOCOL_MAGIC &&
          header.payloadSize == packet.size() - sizeof(network::PacketHeader);
      } catch (const RtypeError&) {
        return false;
      }
    }

    void sendTracked(network::INetworkSocket& socket,
                     const network::PacketData& packet,
                     const network::Endpoint& endpoint) const {
      socket.send(packet, endpoint);
      if (_metrics) _metrics->recordSend(packet.size());
    }

    std::size_t pendingFor(const network::Endpoint& endpoint) const {
      return static_cast<std::size_t>(std::count_if(
        _pending.begin(), _pending.end(), [&endpoint](const Pending& pending) {
          return sameEndpoint(pending.endpoint, endpoint);
        }));
    }

    Iterator find(const network::Endpoint& endpoint,
                  std::uint32_t sequence) {
      return std::find_if(_pending.begin(), _pending.end(),
        [&endpoint, sequence](const Pending& pending) {
          return pending.sequence == sequence &&
            sameEndpoint(pending.endpoint, endpoint);
        });
    }

    std::vector<Pending> _pending;
    std::size_t _pendingBytes{0};
    std::size_t _dropped{0};
    std::size_t _invalid{0};
    std::size_t _retransmitted{0};
    std::size_t _timedOut{0};
    std::size_t _acknowledged{0};
    ServerMetrics* _metrics{nullptr};
  };
}

#endif // !RELIABLEUDPQUEUE_HPP
