#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "GameRoom.hpp"
#include "ReliableUdpQueue.hpp"
#include "ServerMetrics.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <thread>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace rtype::server {

  enum class RoomState : std::uint8_t {
    Waiting = 0,
    Running = 1,
    Full = 2
  };

  struct RoomInfo {
    std::uint32_t id{0};
    std::uint8_t players{0};
    std::uint8_t capacity{0};
    RoomState state{RoomState::Waiting};
    std::uint8_t difficulty{1};
  };

  class ServerNetworkManager {
  public:
    explicit ServerNetworkManager(network::INetworkSocket& socket,
                                  std::size_t maxRooms = 16,
                                  std::size_t defaultCapacity = 4,
                                  std::chrono::milliseconds emptyRoomTtl =
                                    std::chrono::seconds(30))
      : _socket(socket), _reliable(&_metrics),
        _maxRooms(std::clamp<std::size_t>(maxRooms, 1, 128)),
        _defaultCapacity(std::clamp<std::size_t>(defaultCapacity, 1, 4)),
        _emptyRoomTtl(std::max(emptyRoomTtl,
                               std::chrono::milliseconds::zero())) {
      _rooms.emplace(1, RoomEntry{
        std::make_unique<GameRoom>(
          _socket, 1, _defaultCapacity, 1, false, _outgoingSeq,
          &_reliable, &_metrics),
        std::nullopt
      });
    }

    ~ServerNetworkManager() { stopNetworkThread(); }

    void startNetworkThread() {
      if (_receiver.joinable()) return;
      _socket.setNonBlocking(true);
      _stopReceiver.store(false);
      _receiver = std::thread([this] { receiveLoop(); });
    }

    void stopNetworkThread() {
      if (!_receiver.joinable()) return;
      _stopReceiver.store(true);
      _receiver.join();
    }

    void update(float dt) {
      processIncomingPackets();
      const float elapsed = std::isfinite(dt) ? std::max(dt, 0.0f) : 0.0f;
      _reliable.update(_socket, elapsed);
      for (auto& [id, entry] : _rooms) entry.room->update(elapsed);
      cleanupEmptyRooms(elapsed);
      pruneRateLimits();
    }

    [[nodiscard]] std::size_t roomCount() const { return _rooms.size(); }
    [[nodiscard]] std::size_t droppedIncomingPackets() const {
      return _droppedIncomingPackets.load();
    }
    [[nodiscard]] std::size_t pendingReliablePackets() const {
      return _reliable.pendingCount();
    }
    [[nodiscard]] std::size_t retransmittedPackets() const {
      return _reliable.retransmittedCount();
    }
    [[nodiscard]] std::size_t timedOutReliablePackets() const {
      return _reliable.timedOutCount();
    }
    [[nodiscard]] ServerStats stats() const {
      ServerStats result;
      result.receivedDatagrams = _metrics.receivedDatagrams.load();
      result.receivedBytes = _metrics.receivedBytes.load();
      result.sentDatagrams = _metrics.sentDatagrams.load();
      result.sentBytes = _metrics.sentBytes.load();
      result.malformedPackets = _metrics.malformedPackets.load();
      result.rejectedPackets = _metrics.rejectedPackets.load();
      result.rateLimitedPackets = _metrics.rateLimitedPackets.load();
      result.receiveQueueDrops = _metrics.receiveQueueDrops.load();
      result.reliableQueueDrops = _metrics.reliableQueueDrops.load();
      result.reliablePending = _reliable.pendingCount();
      result.reliableAcknowledged = _reliable.acknowledgedCount();
      result.reliableRetransmitted = _reliable.retransmittedCount();
      result.reliableTimedOut = _reliable.timedOutCount();
      result.rooms = _rooms.size();
      for (const auto& [id, entry] : _rooms) {
        result.players += entry.room->playerCount();
        result.droppedSimulationSeconds +=
          entry.room->droppedSimulationSeconds();
      }
      return result;
    }

  private:
    static constexpr std::uint32_t DEFAULT_ROOM_ID = 1;
    static constexpr std::uint8_t MAX_ROOM_PLAYERS = 4;
    static constexpr std::size_t MAX_QUEUED_PACKETS = 4096;
    static constexpr std::size_t MAX_QUEUED_BYTES = 4 * 1024 * 1024;
    static constexpr std::size_t MAX_DATAGRAM_SIZE = 1200;
    static constexpr std::size_t MAX_RATE_LIMIT_ENDPOINTS = 1024;
    static constexpr float PACKET_RATE = 90.0f;
    static constexpr float PACKET_BURST = 120.0f;

    struct ReceivedDatagram {
      network::PacketData data;
      network::Endpoint sender;
    };

    struct RoomEntry {
      std::unique_ptr<GameRoom> room;
      std::optional<std::chrono::steady_clock::time_point> emptySince;
    };

    struct RecentLeave {
      network::NetworkId playerId{0};
      std::uint64_t sessionToken{0};
      std::uint32_t roomId{0};
      std::chrono::steady_clock::time_point expires;
    };

    struct RateLimitState {
      float tokens{PACKET_BURST};
      std::chrono::steady_clock::time_point updated{
        std::chrono::steady_clock::now()};
      std::chrono::steady_clock::time_point lastSeen{
        std::chrono::steady_clock::now()};
    };

    void processIncomingPackets() {
      if (!_receiver.joinable()) {
        receiveAndDispatch();
        return;
      }
      std::deque<ReceivedDatagram> pending;
      {
        std::lock_guard<std::mutex> lock(_queueMutex);
        pending.swap(_pendingPackets);
        _pendingBytes = 0;
      }
      for (const auto& datagram : pending) {
        try {
          dispatchPacket(datagram.data, datagram.sender);
        } catch (const RtypeError&) {
          _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
      }
    }

    void receiveAndDispatch() {
      network::PacketData data;
      network::Endpoint sender;
      while (_socket.receive(data, sender)) {
        _metrics.recordReceive(data.size());
        try {
          dispatchPacket(data, sender);
        } catch (const RtypeError&) {
          _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
      }
    }

    void receiveLoop() {
      while (!_stopReceiver.load()) {
        network::PacketData data;
        network::Endpoint sender;
        try {
          if (!_socket.receive(data, sender)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
          }
        } catch (...) {
          ++_droppedIncomingPackets;
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        _metrics.recordReceive(data.size());
        if (data.size() > MAX_DATAGRAM_SIZE) {
          ++_droppedIncomingPackets;
          _metrics.receiveQueueDrops.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        std::lock_guard<std::mutex> lock(_queueMutex);
        if (_pendingPackets.size() >= MAX_QUEUED_PACKETS ||
            data.size() > MAX_QUEUED_BYTES - _pendingBytes) {
          ++_droppedIncomingPackets;
          _metrics.receiveQueueDrops.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        _pendingBytes += data.size();
        _pendingPackets.push_back({std::move(data), std::move(sender)});
      }
    }

    void dispatchPacket(const network::PacketData& data,
                        const network::Endpoint& sender) {
      if (data.size() < sizeof(network::PacketHeader) ||
          data.size() > MAX_DATAGRAM_SIZE) {
        ++_droppedIncomingPackets;
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;
      if (header.magic != network::PROTOCOL_MAGIC ||
          header.payloadSize != data.size() - sizeof(header)) {
        ++_droppedIncomingPackets;
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      if (!allowPacket(sender)) {
        ++_droppedIncomingPackets;
        _metrics.rateLimitedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }

      if (header.type == network::PacketType::Ack) {
        if (header.payloadSize != sizeof(std::uint32_t)) {
          _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        std::uint32_t acknowledgedSequence{};
        reader >> acknowledgedSequence;
        _reliable.acknowledge(sender, acknowledgedSequence);
        return;
      }

      switch (header.type) {
        case network::PacketType::ConnectReq:
          if (header.payloadSize != 0 && header.payloadSize != 1 &&
              header.payloadSize != 2 && header.payloadSize != 6) {
            _metrics.malformedPackets.fetch_add(1,
                                                std::memory_order_relaxed);
            return;
          }
          handleDefaultConnect(data, sender);
          break;
        case network::PacketType::RoomListReq:
          handleRoomList(sender, header.payloadSize);
          break;
        case network::PacketType::RoomCreateReq:
          handleRoomCreate(reader, sender, header.payloadSize);
          break;
        case network::PacketType::RoomJoinReq:
          handleRoomJoin(reader, sender, header.payloadSize);
          break;
        case network::PacketType::RoomLeaveReq:
          handleLeave(reader, sender, header.payloadSize);
          break;
        case network::PacketType::PlayerInput:
          if (header.payloadSize != sizeof(network::NetworkId) +
              sizeof(std::uint64_t) + 5) {
            _metrics.malformedPackets.fetch_add(1,
                                                std::memory_order_relaxed);
            return;
          }
          routeToExistingRoom(data, sender);
          break;
        case network::PacketType::Disconnect:
          if (header.payloadSize != sizeof(network::NetworkId) +
              sizeof(std::uint64_t)) {
            _metrics.malformedPackets.fetch_add(1,
                                                std::memory_order_relaxed);
            return;
          }
          routeToExistingRoom(data, sender);
          break;
        case network::PacketType::Ping:
          handlePing(reader, header.payloadSize, sender);
          break;
        default:
          _metrics.rejectedPackets.fetch_add(1, std::memory_order_relaxed);
          break;
      }
    }

    void handleDefaultConnect(const network::PacketData& data,
                              const network::Endpoint& sender) {
      if (GameRoom* room = findRoom(sender)) {
        room->handlePacket(data, sender);
        return;
      }
      auto& room = *_rooms.at(DEFAULT_ROOM_ID).room;
      if (!room.canAccept(sender)) {
        sendConnectReject(sender, 1);
        return;
      }
      room.handlePacket(data, sender);
      if (room.containsEndpoint(sender)) {
        _pendingCreates.erase(endpointKey(sender));
      }
      resetEmptyTimer(room.id());
    }

    void handleRoomList(const network::Endpoint& sender,
                        std::uint16_t payloadSize) {
      if (payloadSize != 0) {
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      network::PacketStreamWriter payload;
      const auto infos = roomInfos();
      payload << static_cast<std::uint16_t>(infos.size());
      for (const auto& info : infos) {
        payload << info.id << info.players << info.capacity
                << static_cast<std::uint8_t>(info.state) << info.difficulty;
      }
      sendPacket(network::PacketType::RoomListAck, payload.getBuffer(), sender);
    }

    void handleRoomCreate(network::PacketStreamReader& reader,
                          const network::Endpoint& sender,
                          std::uint16_t payloadSize) {
      if (payloadSize != 2) {
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      if (findRoom(sender)) {
        sendRoomResult(network::PacketType::RoomCreateAck, 0, 1, sender);
        return;
      }
      const std::string key = endpointKey(sender);
      auto pending = _pendingCreates.find(key);
      if (pending != _pendingCreates.end() &&
          _rooms.find(pending->second) != _rooms.end()) {
        sendRoomResult(network::PacketType::RoomCreateAck,
                       pending->second, 0, sender);
        return;
      }
      std::uint8_t capacity{};
      std::uint8_t difficulty{};
      reader >> capacity >> difficulty;
      if (capacity == 0 || capacity > MAX_ROOM_PLAYERS || difficulty > 2) {
        sendRoomResult(network::PacketType::RoomCreateAck, 0, 1, sender);
        return;
      }
      if (_rooms.size() >= _maxRooms) {
        sendRoomResult(network::PacketType::RoomCreateAck, 0, 2, sender);
        return;
      }
      const auto id = allocateRoomId();
      if (id == 0) {
        sendRoomResult(network::PacketType::RoomCreateAck, 0, 2, sender);
        return;
      }
      _rooms.emplace(id, RoomEntry{
        std::make_unique<GameRoom>(
          _socket, id, capacity, difficulty, true, _outgoingSeq,
          &_reliable, &_metrics),
        std::chrono::steady_clock::now()
      });
      _pendingCreates[key] = id;
      sendRoomResult(network::PacketType::RoomCreateAck, id, 0, sender);
    }

    void handleRoomJoin(network::PacketStreamReader& reader,
                        const network::Endpoint& sender,
                        std::uint16_t payloadSize) {
      if (payloadSize != 10) {
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      std::uint32_t roomId{};
      std::uint8_t difficulty{};
      std::uint8_t skin{};
      network::NetworkId previousId{};
      reader >> roomId >> difficulty >> skin >> previousId;
      GameRoom* activeRoom = findRoom(sender);
      if (activeRoom && activeRoom->id() != roomId) {
        sendRoomResult(network::PacketType::RoomJoinAck, roomId, 3, sender);
        return;
      }
      auto it = _rooms.find(roomId);
      if (it == _rooms.end()) {
        sendRoomResult(network::PacketType::RoomJoinAck, roomId, 1, sender);
        return;
      }
      if (activeRoom && activeRoom->id() == roomId) {
        sendRoomResult(network::PacketType::RoomJoinAck, roomId, 0, sender);
      } else if (!it->second.room->canAccept(sender)) {
        sendRoomResult(network::PacketType::RoomJoinAck, roomId, 2, sender);
        return;
      } else {
        sendRoomResult(network::PacketType::RoomJoinAck, roomId, 0, sender);
      }
      if (difficulty > 2 && difficulty != 0xFF) difficulty = 1;
      network::PacketHeader connectHeader{
        network::PROTOCOL_MAGIC, network::PacketType::ConnectReq, 0, 6
      };
      network::PacketStreamWriter connect;
      connect << connectHeader << difficulty << skin << previousId;
      it->second.room->handlePacket(connect.getBuffer(), sender);
      it->second.emptySince.reset();
      _pendingCreates.erase(endpointKey(sender));
    }

    void handleLeave(network::PacketStreamReader& reader,
                     const network::Endpoint& sender,
                     std::uint16_t payloadSize) {
      if (payloadSize != sizeof(network::NetworkId) + sizeof(std::uint64_t)) {
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      network::NetworkId playerId{};
      std::uint64_t sessionToken{};
      reader >> playerId >> sessionToken;
      const std::string key = endpointKey(sender);
      const auto now = std::chrono::steady_clock::now();
      auto recent = _recentLeaves.find(key);
      if (recent != _recentLeaves.end() && recent->second.expires > now &&
          recent->second.playerId == playerId &&
          recent->second.sessionToken == sessionToken) {
        sendRoomResult(network::PacketType::RoomLeaveAck,
                       recent->second.roomId, 0, sender);
        return;
      }
      GameRoom* room = findRoom(sender);
      if (!room || !room->containsClient(playerId, sender) ||
          !room->validateSession(playerId, sender, sessionToken)) {
        sendRoomResult(network::PacketType::RoomLeaveAck,
                       room ? room->id() : 0, 1, sender);
        return;
      }
      const std::uint32_t roomId = room->id();
      network::PacketHeader disconnectHeader{
        network::PROTOCOL_MAGIC, network::PacketType::Disconnect, 0,
        static_cast<std::uint16_t>(sizeof(playerId) + sizeof(sessionToken))
      };
      network::PacketStreamWriter disconnect;
      disconnect << disconnectHeader << playerId << sessionToken;
      room->handlePacket(disconnect.getBuffer(), sender);
      _recentLeaves[key] = RecentLeave{
        playerId, sessionToken, roomId, now + std::chrono::seconds(10)
      };
      sendRoomResult(network::PacketType::RoomLeaveAck, roomId, 0, sender);
    }

    void handlePing(network::PacketStreamReader& reader,
                    std::uint16_t payloadSize,
                    const network::Endpoint& sender) {
      if (payloadSize != sizeof(std::uint64_t)) {
        _metrics.malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      std::uint64_t timestamp{};
      reader >> timestamp;
      network::PacketStreamWriter payload;
      payload << timestamp;
      sendPacket(network::PacketType::Pong, payload.getBuffer(), sender);
    }

    void routeToExistingRoom(const network::PacketData& data,
                             const network::Endpoint& sender) {
      GameRoom* room = findRoom(sender);
      if (!room) {
        _metrics.rejectedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      room->handlePacket(data, sender);
    }

    static std::string endpointKey(const network::Endpoint& endpoint) {
      return endpoint.address + '\x1f' + std::to_string(endpoint.port);
    }

    GameRoom* findRoom(const network::Endpoint& endpoint) {
      for (auto& [id, entry] : _rooms) {
        if (entry.room->containsEndpoint(endpoint)) return entry.room.get();
      }
      return nullptr;
    }

    std::uint32_t allocateRoomId() {
      for (std::size_t attempt = 0; attempt < _maxRooms + 1; ++attempt) {
        const std::uint32_t candidate = _nextRoomId++;
        if (_nextRoomId == 0) _nextRoomId = 2;
        if (candidate != 0 && _rooms.find(candidate) == _rooms.end())
          return candidate;
      }
      return 0;
    }

    std::vector<RoomInfo> roomInfos() const {
      std::vector<RoomInfo> result;
      result.reserve(_rooms.size());
      for (const auto& [id, entry] : _rooms) {
        const auto& room = *entry.room;
        const auto players = room.playerCount();
        const auto state = room.isFull() ? RoomState::Full
          : players == 0 ? RoomState::Waiting : RoomState::Running;
        result.push_back(RoomInfo{
          id, static_cast<std::uint8_t>(players),
          static_cast<std::uint8_t>(room.maxPlayers()), state,
          room.difficulty()
        });
      }
      std::sort(result.begin(), result.end(),
        [](const RoomInfo& lhs, const RoomInfo& rhs) {
          return lhs.id < rhs.id;
        });
      return result;
    }

    void cleanupEmptyRooms(float) {
      const auto now = std::chrono::steady_clock::now();
      for (auto it = _rooms.begin(); it != _rooms.end();) {
        if (it->first == DEFAULT_ROOM_ID) {
          ++it;
          continue;
        }
        if (!it->second.room->isEmpty()) {
          it->second.emptySince.reset();
          ++it;
          continue;
        }
        if (!it->second.emptySince) it->second.emptySince = now;
        if (now - *it->second.emptySince >= _emptyRoomTtl) {
          const auto expiredId = it->first;
          it = _rooms.erase(it);
          for (auto pending = _pendingCreates.begin();
               pending != _pendingCreates.end();) {
            if (pending->second == expiredId)
              pending = _pendingCreates.erase(pending);
            else
              ++pending;
          }
        } else {
          ++it;
        }
      }
    }

    void resetEmptyTimer(std::uint32_t roomId) {
      auto it = _rooms.find(roomId);
      if (it != _rooms.end()) it->second.emptySince.reset();
    }

    void sendConnectReject(const network::Endpoint& recipient,
                           std::uint8_t reason) {
      network::PacketStreamWriter payload;
      payload << reason;
      sendPacket(network::PacketType::ConnectReject, payload.getBuffer(), recipient);
    }

    void sendRoomResult(network::PacketType type, std::uint32_t roomId,
                        std::uint8_t status,
                        const network::Endpoint& recipient) {
      network::PacketStreamWriter payload;
      payload << roomId << status;
      sendPacket(type, payload.getBuffer(), recipient);
    }

    void sendPacket(network::PacketType type,
                    const network::PacketData& payload,
                    const network::Endpoint& recipient) {
      if (payload.size() > UINT16_MAX ||
          payload.size() + sizeof(network::PacketHeader) > MAX_DATAGRAM_SIZE) {
        _metrics.rejectedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++,
        static_cast<std::uint16_t>(payload.size())
      };
      network::PacketStreamWriter writer;
      writer << header;
      auto packet = writer.getBuffer();
      packet.insert(packet.end(), payload.begin(), payload.end());
      if (ReliableUdpQueue::isReliable(type)) {
        _reliable.send(_socket, packet, recipient);
      } else {
        _socket.send(packet, recipient);
        _metrics.recordSend(packet.size());
      }
    }

    bool allowPacket(const network::Endpoint& endpoint) {
      const auto now = std::chrono::steady_clock::now();
      const std::string key = endpointKey(endpoint);
      auto it = _rateLimits.find(key);
      if (it == _rateLimits.end()) {
        if (_rateLimits.size() >= MAX_RATE_LIMIT_ENDPOINTS) return false;
        it = _rateLimits.emplace(key, RateLimitState{}).first;
      }
      auto& state = it->second;
      const float elapsed = std::chrono::duration<float>(
        now - state.updated).count();
      state.tokens = std::min(PACKET_BURST,
                              state.tokens + elapsed * PACKET_RATE);
      state.updated = now;
      state.lastSeen = now;
      if (state.tokens < 1.0f) return false;
      state.tokens -= 1.0f;
      return true;
    }

    void pruneRateLimits() {
      const auto now = std::chrono::steady_clock::now();
      for (auto it = _rateLimits.begin(); it != _rateLimits.end();) {
        const auto age = now - it->second.lastSeen;
        if (age > std::chrono::minutes(1)) it = _rateLimits.erase(it);
        else ++it;
      }
      for (auto it = _recentLeaves.begin(); it != _recentLeaves.end();) {
        if (it->second.expires <= now) it = _recentLeaves.erase(it);
        else ++it;
      }
    }

    network::INetworkSocket& _socket;
    ServerMetrics _metrics;
    ReliableUdpQueue _reliable;
    std::uint32_t _outgoingSeq{1};
    std::unordered_map<std::uint32_t, RoomEntry> _rooms;
    std::size_t _maxRooms{16};
    std::size_t _defaultCapacity{4};
    std::chrono::milliseconds _emptyRoomTtl{std::chrono::seconds(30)};
    std::uint32_t _nextRoomId{2};
    std::unordered_map<std::string, std::uint32_t> _pendingCreates;
    std::unordered_map<std::string, RecentLeave> _recentLeaves;
    std::atomic<bool> _stopReceiver{false};
    std::thread _receiver;
    std::mutex _queueMutex;
    std::deque<ReceivedDatagram> _pendingPackets;
    std::size_t _pendingBytes{0};
    std::atomic<std::size_t> _droppedIncomingPackets{0};
    std::unordered_map<std::string, RateLimitState> _rateLimits;
  };

}

#endif // !SERVERNETWORKMANAGER_HPP
