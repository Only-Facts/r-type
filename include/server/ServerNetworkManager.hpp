#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "GameRoom.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <string>
#include <memory>
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
                                  std::size_t defaultCapacity = 4)
      : _socket(socket), _maxRooms(std::clamp<std::size_t>(maxRooms, 1, 256)),
        _defaultCapacity(std::clamp<std::size_t>(defaultCapacity, 1, 4)) {
      _rooms.emplace(1, RoomEntry{
        std::make_unique<GameRoom>(
          _socket, 1, _defaultCapacity, 1, false, _outgoingSeq),
        0.0f
      });
    }

    ~ServerNetworkManager() { stopNetworkThread(); }

    void startNetworkThread() {
      if (_receiver.joinable()) return;
      _socket.setNonBlocking(true);
      _receiver = std::jthread([this](std::stop_token stop) {
        receiveLoop(stop);
      });
    }

    void stopNetworkThread() {
      if (!_receiver.joinable()) return;
      _receiver.request_stop();
      _receiver.join();
    }

    void update(float dt) {
      processIncomingPackets();
      const float elapsed = std::max(dt, 0.0f);
      for (auto& [id, entry] : _rooms) entry.room->update(elapsed);
      cleanupEmptyRooms(elapsed);
    }

    [[nodiscard]] std::size_t roomCount() const { return _rooms.size(); }
    [[nodiscard]] std::size_t droppedIncomingPackets() const {
      return _droppedIncomingPackets.load();
    }

  private:
    static constexpr std::uint32_t DEFAULT_ROOM_ID = 1;
    static constexpr float EMPTY_ROOM_TTL = 30.0f;
    static constexpr std::uint8_t MAX_ROOM_PLAYERS = 4;
    static constexpr std::size_t MAX_QUEUED_PACKETS = 4096;
    static constexpr std::size_t MAX_QUEUED_BYTES = 4 * 1024 * 1024;

    struct ReceivedDatagram {
      network::PacketData data;
      network::Endpoint sender;
    };

    struct RoomEntry {
      std::unique_ptr<GameRoom> room;
      float emptyTime{0.0f};
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
          continue;
        }
      }
    }

    void receiveAndDispatch() {
      network::PacketData data;
      network::Endpoint sender;
      while (_socket.receive(data, sender)) {
        try {
          dispatchPacket(data, sender);
        } catch (const RtypeError&) {
          continue;
        }
      }
    }

    void receiveLoop(std::stop_token stop) {
      while (!stop.stop_requested()) {
        network::PacketData data;
        network::Endpoint sender;
        if (!_socket.receive(data, sender)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        std::lock_guard<std::mutex> lock(_queueMutex);
        if (_pendingPackets.size() >= MAX_QUEUED_PACKETS ||
            data.size() > MAX_QUEUED_BYTES - _pendingBytes) {
          ++_droppedIncomingPackets;
          continue;
        }
        _pendingBytes += data.size();
        _pendingPackets.push_back({std::move(data), std::move(sender)});
      }
    }

    void dispatchPacket(const network::PacketData& data,
                        const network::Endpoint& sender) {
      if (data.size() < sizeof(network::PacketHeader)) return;
      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;
      if (header.magic != network::PROTOCOL_MAGIC) return;
      if (header.payloadSize != data.size() - sizeof(header)) return;

      switch (header.type) {
        case network::PacketType::ConnectReq:
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
        case network::PacketType::Disconnect:
          routeToExistingRoom(data, sender);
          break;
        default:
          routeToExistingRoom(data, sender);
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
      if (payloadSize != 0) return;
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
      if (payloadSize != 2) return;
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
          _socket, id, capacity, difficulty, true, _outgoingSeq),
        0.0f
      });
      _pendingCreates[key] = id;
      sendRoomResult(network::PacketType::RoomCreateAck, id, 0, sender);
    }

    void handleRoomJoin(network::PacketStreamReader& reader,
                        const network::Endpoint& sender,
                        std::uint16_t payloadSize) {
      if (payloadSize != 10) return;
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
      it->second.emptyTime = 0.0f;
      _pendingCreates.erase(endpointKey(sender));
    }

    void handleLeave(network::PacketStreamReader& reader,
                     const network::Endpoint& sender,
                     std::uint16_t payloadSize) {
      if (payloadSize != sizeof(network::NetworkId)) return;
      network::NetworkId playerId{};
      reader >> playerId;
      GameRoom* room = findRoom(sender);
      if (!room || !room->containsClient(playerId, sender)) {
        sendRoomResult(network::PacketType::RoomLeaveAck,
                       room ? room->id() : 0, 1, sender);
        return;
      }
      network::PacketHeader disconnectHeader{
        network::PROTOCOL_MAGIC, network::PacketType::Disconnect, 0,
        static_cast<std::uint16_t>(sizeof(playerId))
      };
      network::PacketStreamWriter disconnect;
      disconnect << disconnectHeader << playerId;
      room->handlePacket(disconnect.getBuffer(), sender);
      sendRoomResult(network::PacketType::RoomLeaveAck, room->id(), 0, sender);
    }

    void routeToExistingRoom(const network::PacketData& data,
                             const network::Endpoint& sender) {
      if (GameRoom* room = findRoom(sender)) room->handlePacket(data, sender);
    }

    static std::string endpointKey(const network::Endpoint& endpoint) {
      return endpoint.address + ":" + std::to_string(endpoint.port);
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

    void cleanupEmptyRooms(float dt) {
      for (auto it = _rooms.begin(); it != _rooms.end();) {
        if (it->first == DEFAULT_ROOM_ID || !it->second.room->isEmpty()) {
          it->second.emptyTime = 0.0f;
          ++it;
          continue;
        }
        it->second.emptyTime += std::max(dt, 0.0f);
        if (it->second.emptyTime >= EMPTY_ROOM_TTL) {
          const auto expiredId = it->first;
          it = _rooms.erase(it);
          for (auto pending = _pendingCreates.begin();
               pending != _pendingCreates.end();) {
            if (pending->second == expiredId) pending = _pendingCreates.erase(pending);
            else ++pending;
          }
        } else ++it;
      }
    }

    void resetEmptyTimer(std::uint32_t roomId) {
      auto it = _rooms.find(roomId);
      if (it != _rooms.end()) it->second.emptyTime = 0.0f;
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
      if (payload.size() > UINT16_MAX) return;
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++,
        static_cast<std::uint16_t>(payload.size())
      };
      network::PacketStreamWriter writer;
      writer << header;
      auto packet = writer.getBuffer();
      packet.insert(packet.end(), payload.begin(), payload.end());
      _socket.send(packet, recipient);
    }

    network::INetworkSocket& _socket;
    std::uint32_t _outgoingSeq{1};
    std::unordered_map<std::uint32_t, RoomEntry> _rooms;
    std::size_t _maxRooms{16};
    std::size_t _defaultCapacity{4};
    std::uint32_t _nextRoomId{2};
    std::unordered_map<std::string, std::uint32_t> _pendingCreates;
    std::jthread _receiver;
    std::mutex _queueMutex;
    std::deque<ReceivedDatagram> _pendingPackets;
    std::size_t _pendingBytes{0};
    std::atomic<std::size_t> _droppedIncomingPackets{0};
  };

}

#endif // !SERVERNETWORKMANAGER_HPP
