#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "engine/core/SnapshotClock.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/ecs/SnapshotBuffer.hpp"
#include "exception.hpp"
#include "network/INetworkSocket.hpp"
#include "network/PacketStream.hpp"
#include "network/Protocol.hpp"
#include "network/SnapshotDelta.hpp"
#include "network/ProtocolExtension.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client {
  class ClientNetworkManager {
  public:
    using SpawnCallback =
      std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType, bool isLocal)>;
    using DestroyCallback = std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType)>;
    using HealthCallback =
      std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType, int, int, int)>;
    using AugmentCallback = std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType,
                                               network::ext::AugmentId, int, float)>;
    using ScoreCallback = std::function<void(network::NetworkId, std::uint32_t)>;

    struct InputAck {
      std::uint32_t lastInput{0};
      std::uint32_t tick{0};
      float x{0.0f};
      float y{0.0f};
    };
    using InputAckCallback = std::function<void(const InputAck&)>;

    enum class SessionState : std::uint8_t {
      Disconnected,
      Lobby,
      Connecting,
      InGame
    };

    enum class DisconnectReason : std::uint8_t {
      None,
      Left,
      Rejected,
      TimedOut
    };

    struct RoomInfo {
      std::uint32_t id{0};
      std::uint8_t players{0};
      std::uint8_t capacity{0};
      std::uint8_t state{0};
      std::uint8_t difficulty{0};
    };

    enum class RoomResult : std::uint8_t {
      Success,
      NotFound,
      Full,
      AlreadyInRoom,
      InvalidSettings,
      LimitReached,
      TimedOut,
      Unknown
    };

    using RoomListCallback = std::function<void(RoomResult, const std::vector<RoomInfo>&)>;
    using RoomCallback = std::function<void(RoomResult, std::uint32_t roomId)>;

    ClientNetworkManager(network::INetworkSocket& socket, engine::Registry& registry)
      : _socket(socket), _registry(registry) {}

    void setOnEntitySpawned(SpawnCallback callback) { _onSpawn = std::move(callback); }

    void setOnEntityDestroyed(DestroyCallback callback) { _onDestroy = std::move(callback); }

    void setOnHealthChanged(HealthCallback callback) { _onHealth = std::move(callback); }
    void setOnAugmentChanged(AugmentCallback callback) { _onAugment = std::move(callback); }
    void setOnScoreChanged(ScoreCallback callback) { _onScore = std::move(callback); }
    void setOnInputAck(InputAckCallback callback) { _onInputAck = std::move(callback); }
    void setOnRoomList(RoomListCallback callback) { _onRoomList = std::move(callback); }
    void setOnRoomCreated(RoomCallback callback) { _onRoomCreated = std::move(callback); }
    void setOnRoomJoined(RoomCallback callback) { _onRoomJoined = std::move(callback); }

    static constexpr std::uint8_t NO_PREFERENCE = 0xFF;

    void openLobby(const std::string& host, unsigned short port) {
      if (_state == SessionState::Connecting || _state == SessionState::InGame) return;
      openSocket(host, port);
      _state = SessionState::Lobby;
      _disconnectReason = DisconnectReason::None;
    }

    void connectToServer(const std::string& host, unsigned short port,
                         std::optional<std::uint8_t> difficulty = std::nullopt,
                         std::optional<std::uint8_t> skin = std::nullopt,
                         std::optional<network::NetworkId> previousNetId = std::nullopt) {
      openSocket(host, port);

      const std::uint16_t payloadSize = previousNetId ? 6 : skin ? 2 : difficulty ? 1 : 0;
      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::ConnectReq, _outgoingSeq++,
                                   payloadSize};
      network::PacketStreamWriter writer;
      writer << header;
      if (difficulty || skin || previousNetId) writer << difficulty.value_or(NO_PREFERENCE);
      if (skin || previousNetId) writer << skin.value_or(NO_PREFERENCE);
      if (previousNetId) writer << *previousNetId;
      startConnecting(writer.getBuffer());
    }

    bool requestRooms() {
      if (_state != SessionState::Lobby) return false;
      startLobbyRequest(_roomListRequest, network::PacketType::RoomListReq, {});
      return true;
    }

    bool createRoom(std::uint8_t capacity, std::uint8_t difficulty) {
      if (_state != SessionState::Lobby) return false;
      network::PacketStreamWriter payload;
      payload << capacity << difficulty;
      startLobbyRequest(_roomCreateRequest, network::PacketType::RoomCreateReq, payload.getBuffer());
      return true;
    }

    bool joinRoom(const std::string& host, unsigned short port, std::uint32_t roomId,
                  std::optional<std::uint8_t> difficulty = std::nullopt,
                  std::optional<std::uint8_t> skin = std::nullopt,
                  std::optional<network::NetworkId> previousNetId = std::nullopt) {
      if (_state == SessionState::Connecting || _state == SessionState::InGame) return false;
      openSocket(host, port);
      network::PacketStreamWriter payload;
      payload << roomId << difficulty.value_or(NO_PREFERENCE) << skin.value_or(NO_PREFERENCE)
              << previousNetId.value_or(network::NetworkId{0});
      startConnecting(buildPacket(network::PacketType::RoomJoinReq, payload.getBuffer()));
      _joinRoomId = roomId;
      return true;
    }

    void update(float dt) {
      if (_state == SessionState::Connecting && !_connectPacket.empty()) {
        _connectRetryTimer -= std::max(dt, 0.0f);
        if (_connectRetryTimer <= 0.0f) {
          if (_connectAttempts >= MAX_CONNECT_RETRIES) {
            const std::optional<std::uint32_t> unansweredJoin = _joinAccepted ? std::nullopt : _joinRoomId;
            endSession(DisconnectReason::TimedOut);
            if (unansweredJoin && _onRoomJoined) _onRoomJoined(RoomResult::TimedOut, *unansweredJoin);
          } else {
            _socket.send(_connectPacket, _serverEndpoint);
            ++_connectAttempts;
            _connectRetryTimer = CONNECT_RETRY_INTERVAL;
          }
        }
      }
      if (retryLobbyRequest(_roomListRequest, dt) && _onRoomList) _onRoomList(RoomResult::TimedOut, {});
      if (retryLobbyRequest(_roomCreateRequest, dt) && _onRoomCreated) _onRoomCreated(RoomResult::TimedOut, 0);
      processIncomingPackets();
      flushPendingDestroys(dt);
      updatePendingDeltas(dt);
    }

    std::optional<std::uint32_t> sendInputs(bool up, bool down, bool left, bool right, bool shoot) {
      if (!isConnected()) return std::nullopt;

      const std::uint32_t inputSequence = _outgoingSeq++;
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::PlayerInput, inputSequence, 0
      };

      network::PacketStreamWriter payload;
      payload << _localNetId << _sessionToken
              << static_cast<std::uint8_t>(up)
              << static_cast<std::uint8_t>(down)
              << static_cast<std::uint8_t>(left)
              << static_cast<std::uint8_t>(right)
              << static_cast<std::uint8_t>(shoot);

      header.payloadSize = static_cast<std::uint16_t>(payload.getBuffer().size());

      network::PacketStreamWriter finalPacket;
      finalPacket << header;

      network::PacketData packet = finalPacket.getBuffer();
      packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());

      _socket.send(packet, _serverEndpoint);
      return inputSequence;
    }

    void disconnect() {
      if (_state == SessionState::Disconnected) return;

      if (_state == SessionState::InGame) {
        network::PacketHeader header{
          network::PROTOCOL_MAGIC, network::PacketType::Disconnect, _outgoingSeq++,
          static_cast<std::uint16_t>(sizeof(_localNetId) + sizeof(_sessionToken))
        };
        network::PacketStreamWriter writer;
        writer << header << _localNetId << _sessionToken;
        _socket.send(writer.getBuffer(), _serverEndpoint);
      }
      endSession(DisconnectReason::Left);
    }

    [[nodiscard]] SessionState getState() const { return _state; }
    [[nodiscard]] DisconnectReason getDisconnectReason() const { return _disconnectReason; }
    [[nodiscard]] bool isConnected() const { return _state == SessionState::InGame; }
    [[nodiscard]] network::NetworkId getLocalNetId() const { return _localNetId; }
    void setSnapshotClock(engine::SnapshotClock* clock) { _snapshotClock = clock; }

    void setSnapshotBufferSize(std::size_t size) {
      _snapshotBufferSize = std::max(size, engine::MIN_SNAPSHOT_BUFFER_CAPACITY);
    }

    [[nodiscard]] std::size_t getLateSnapshotCount() const { return _lateSnapshots; }
    [[nodiscard]] std::size_t getUnknownBaseDeltaCount() const { return _unknownBaseDeltas; }
    [[nodiscard]] std::size_t getFullSnapshotRequestCount() const { return _fullSnapshotRequests; }
    [[nodiscard]] std::size_t getFullSnapshotCount() const { return _fullSnapshotsReceived; }
    [[nodiscard]] std::size_t getDeltaSnapshotCount() const { return _deltaSnapshotsReceived; }
    [[nodiscard]] std::size_t getSnapshotBytesReceived() const { return _snapshotBytesReceived; }

    [[nodiscard]] std::optional<std::uint32_t> getLastSnapshotTick() const {
      if (!_hasSnapshotTick) return std::nullopt;
      return _lastSnapshotTick;
    }
    [[nodiscard]] std::optional<std::uint8_t> getServerDifficulty() const { return _serverDifficulty; }
    [[nodiscard]] std::optional<std::uint8_t> getPlayerSkin(network::NetworkId netId) const {
      auto it = _playerSkins.find(netId);
      if (it == _playerSkins.end()) return std::nullopt;
      return it->second;
    }
    [[nodiscard]] std::size_t getDroppedPacketCount() const { return _droppedPackets; }

  private:
    struct RemoteEntity {
      engine::Entity entity;
      network::ext::EntityType type;
    };

    struct PendingDestroy {
      engine::Entity entity;
      network::NetworkId netId;
      network::ext::EntityType type;
      std::uint32_t tick;
      float waited;
    };

    static constexpr float MAX_DEFERRED_DESTROY_SECONDS = 1.0f;

    struct PendingHealth {
      int health{0};
      int maximum{0};
      std::uint32_t sequence{0};
    };

    struct PendingAugment {
      network::ext::AugmentId augment{0};
      int level{0};
      float remaining{0.0f};
      std::uint32_t sequence{0};
    };

    struct PendingPlayerSpawn {
      network::NetworkId netId{0};
      float x{0.0f};
      float y{0.0f};
      std::uint8_t skin{NO_PREFERENCE};
      std::uint32_t sequence{0};
    };

    void flushPendingPlayerSpawns() {
      for (const auto& spawn : _pendingPlayerSpawns) {
        spawnEntity(spawn.netId, network::ext::ENTITY_PLAYER,
                    spawn.x, spawn.y, spawn.sequence);
      }
      _pendingPlayerSpawns.clear();
    }

    void processIncomingPackets() {
      network::PacketData data;
      network::Endpoint sender;

      while (_socket.receive(data, sender)) {
        if (sender.port != _serverEndpoint.port) {
          ++_droppedPackets;
          continue;
        }
        try {
          handlePacket(data);
        } catch (const RtypeError&) {
          ++_droppedPackets;
        }
      }
    }

    void handlePacket(const network::PacketData& data) {
      if (data.size() < sizeof(network::PacketHeader)) return;

      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;

      if (header.magic != network::PROTOCOL_MAGIC ||
          header.payloadSize != data.size() - sizeof(header)) return;
      if (header.type == network::PacketType::Ack) return;
      if (isReliableServerPacket(header.type)) {
        sendAcknowledgement(header.sequence);
        if (!rememberReliableSequence(header.sequence)) return;
      }
      const bool isSync =
        header.type == network::PacketType::GameStateSync ||
        header.type == network::PacketType::GameStateSyncPacked ||
        header.type == network::PacketType::GameStateSyncDelta;
      const bool staleSync = isSync && !isSequenceNewer(header.sequence, _lastSyncSeq, _hasSyncSequence);
      if (staleSync && header.type == network::PacketType::GameStateSync) return;

      if (header.type == network::ext::SPAWN_ENTITY) {
        network::NetworkId netId;
        network::ext::EntityType type;
        float x, y;
        reader >> netId >> type >> x >> y;
        spawnEntity(netId, type, x, y, header.sequence);
        return;
      }
      if (header.type == network::ext::HEALTH_UPDATE) {
        network::NetworkId netId;
        std::uint8_t health, maxHealth;
        reader >> netId >> health >> maxHealth;
        updateHealth(netId, health, maxHealth, header.sequence);
        return;
      }
      if (header.type == network::ext::AUGMENT_UPDATE) {
        network::NetworkId netId;
        network::ext::AugmentId augment;
        std::uint8_t level;
        float remaining;
        reader >> netId >> augment >> level >> remaining;
        const auto key = augmentKey(netId, augment);
        auto it = _networkToEcs.find(netId);
        if (it != _networkToEcs.end() && _onAugment) {
          const auto last = _lastAugmentSequences.find(key);
          if (last != _lastAugmentSequences.end() &&
              !isSequenceNewer(header.sequence, last->second, true)) return;
          _lastAugmentSequences[key] = header.sequence;
          _pendingAugments.erase(key);
          _onAugment(it->second.entity, netId, it->second.type, augment,
                     level, remaining);
        } else {
          auto pending = _pendingAugments.find(key);
          if (pending == _pendingAugments.end() ||
              isSequenceNewer(header.sequence, pending->second.sequence, true)) {
            _pendingAugments[key] = PendingAugment{
              augment, level, remaining, header.sequence};
          }
        }
        return;
      }
      if (header.type == network::PacketType::InputAck) {
        InputAck ack;
        reader >> ack.lastInput >> ack.tick >> ack.x >> ack.y;
        if (!isConnected() || !isSequenceNewer(ack.lastInput, _lastInputAck, _hasInputAck)) return;
        _lastInputAck = ack.lastInput;
        _hasInputAck = true;
        if (_onInputAck) _onInputAck(ack);
        return;
      }
      if (header.type == network::ext::SCORE_UPDATE) {
        network::NetworkId netId;
        std::uint32_t score;
        reader >> netId >> score;
        const auto last = _lastScoreSequences.find(netId);
        if (last != _lastScoreSequences.end() &&
            !isSequenceNewer(header.sequence, last->second, true)) return;
        _lastScoreSequences[netId] = header.sequence;
        if (_onScore) _onScore(netId, score);
        return;
      }

      switch (header.type) {
        case network::PacketType::ConnectReject:
          if (_state != SessionState::Connecting) break;
          endSession(DisconnectReason::Rejected);
          _pendingPlayerSpawns.clear();
          break;
        case network::PacketType::ConnectAck: {
          network::NetworkId netId{};
          std::optional<std::uint8_t> difficulty;
          std::uint64_t token{0};
          reader >> netId;
          if (reader.hasMoreData()) {
            std::uint8_t value = 0;
            reader >> value;
            difficulty = value;
          }
          if (reader.hasMoreData()) reader >> token;
          if (token == 0 || (_state != SessionState::Connecting && _state != SessionState::InGame)) return;
          _localNetId = netId;
          if (difficulty) _serverDifficulty = difficulty;
          _sessionToken = token;
          _connectPacket.clear();
          _state = SessionState::InGame;
          flushPendingPlayerSpawns();
          break;
        }
        case network::PacketType::SpawnPlayer: {
          PendingPlayerSpawn spawn{};
          spawn.sequence = header.sequence;
          reader >> spawn.netId >> spawn.x >> spawn.y;
          if (reader.hasMoreData()) {
            spawn.skin = NO_PREFERENCE;
            reader >> spawn.skin;
          }
          if (spawn.skin != NO_PREFERENCE)
            _playerSkins[spawn.netId] = spawn.skin;
          if (!isConnected()) {
            if (_pendingPlayerSpawns.size() < MAX_PENDING_PLAYER_SPAWNS)
              _pendingPlayerSpawns.push_back(spawn);
            break;
          }
          spawnEntity(spawn.netId, network::ext::ENTITY_PLAYER,
                      spawn.x, spawn.y, spawn.sequence);
          break;
        }
        case network::PacketType::DestroyEntity: {
          network::NetworkId netId;
          reader >> netId;
          destroyEntity(netId, header.sequence);
          break;
        }
        case network::PacketType::GameStateSync: {
          std::uint16_t count{};
          reader >> count;
          if (count > MAX_SNAPSHOT_COUNT) return;
          if (reader.remaining() != static_cast<std::size_t>(count) * sizeof(network::EntityStateData)) return;
          for (std::uint16_t i = 0; i < count; ++i) {
            network::EntityStateData state{};
            reader >> state;
            if (!staleSync) updateEntityState(state);
          }
          if (staleSync) break;
          _lastSyncSeq = header.sequence;
          _hasSyncSequence = true;
          break;
        }
        case network::PacketType::GameStateSyncPacked: {
          std::uint32_t tick{0};
          std::uint16_t count{};
          reader >> tick >> count;
          if (count > MAX_SNAPSHOT_COUNT) return;
          if (reader.remaining() != static_cast<std::size_t>(count) * sizeof(network::PackedEntityStateData)) return;
          network::SnapshotState state;
          state.reserve(count);
          for (std::uint16_t i = 0; i < count; ++i) {
            network::PackedEntityStateData packed{};
            reader >> packed;
            state[packed.netId] = packed;
          }
          _awaitingFullSnapshot = false;
          ++_fullSnapshotsReceived;
          _snapshotBytesReceived += data.size();
          acceptSnapshot(tick, std::move(state));
          break;
        }
        case network::PacketType::GameStateSyncDelta:
          handleSnapshotDelta(network::readSnapshotDelta(reader, MAX_SNAPSHOT_COUNT));
          ++_deltaSnapshotsReceived;
          _snapshotBytesReceived += data.size();
          break;
        case network::PacketType::RoomListAck:
          handleRoomList(reader);
          break;
        case network::PacketType::RoomCreateAck:
          if (header.payloadSize == ROOM_RESULT_SIZE) handleRoomCreate(reader);
          break;
        case network::PacketType::RoomJoinAck:
          if (header.payloadSize == ROOM_RESULT_SIZE) handleRoomJoin(reader);
          break;
        default: break;
      }
    }

    static constexpr std::size_t MAX_SNAPSHOT_COUNT = 256;
    static constexpr std::size_t MAX_SNAPSHOT_HISTORY = 64;
    static constexpr std::size_t MAX_PENDING_DELTAS = 16;
    static constexpr float PENDING_DELTA_TIMEOUT = 0.1f;
    static constexpr float FULL_REQUEST_INTERVAL = 0.25f;

    struct PendingDelta {
      network::SnapshotDelta delta;
      float waited;
    };

    void acceptSnapshot(std::uint32_t tick, network::SnapshotState state) {
      const bool late = _hasSnapshotTick && tick <= _lastSnapshotTick;
      for (const auto& [id, packed] : state) {
        const network::EntityStateData entity = network::unpackEntityState(packed);
        recordSnapshotSample(entity, tick);
        if (!late) updateEntityState(entity);
      }
      if (_snapshotClock) _snapshotClock->observe(tick);
      if (late) {
        ++_lateSnapshots;
      } else {
        _lastSnapshotTick = tick;
        _hasSnapshotTick = true;
      }
      storeSnapshot(tick, std::move(state));
      resolvePendingDeltas();
    }

    void handleSnapshotDelta(const network::SnapshotDelta& delta) {
      if (findSnapshot(delta.tick)) return;
      const network::SnapshotState* base = findSnapshot(delta.baseTick);
      if (!base) {
        ++_unknownBaseDeltas;
        if (_pendingDeltas.size() >= MAX_PENDING_DELTAS) {
          _pendingDeltas.clear();
          _awaitingFullSnapshot = true;
          requestFullSnapshot();
        }
        _pendingDeltas.push_back({delta, 0.0f});
        return;
      }
      acceptSnapshot(delta.tick, network::applySnapshotDelta(*base, delta));
      resolvePendingDeltas();
    }

    void resolvePendingDeltas() {
      bool progressed = true;
      while (progressed) {
        progressed = false;
        for (auto pending = _pendingDeltas.begin(); pending != _pendingDeltas.end(); ++pending) {
          if (findSnapshot(pending->delta.tick)) {
            _pendingDeltas.erase(pending);
            progressed = true;
            break;
          }
          const network::SnapshotState* base = findSnapshot(pending->delta.baseTick);
          if (!base) continue;
          const network::SnapshotDelta delta = pending->delta;
          _pendingDeltas.erase(pending);
          acceptSnapshot(delta.tick, network::applySnapshotDelta(*base, delta));
          progressed = true;
          break;
        }
      }
    }

    void updatePendingDeltas(float dt) {
      const float elapsed = std::max(dt, 0.0f);
      _fullRequestCooldown = std::max(0.0f, _fullRequestCooldown - elapsed);
      bool expired = false;
      for (auto& pending : _pendingDeltas) {
        pending.waited += elapsed;
        if (pending.waited >= PENDING_DELTA_TIMEOUT) expired = true;
      }
      if (expired) {
        _pendingDeltas.clear();
        _awaitingFullSnapshot = true;
      }
      if (_awaitingFullSnapshot) requestFullSnapshot();
    }

    void requestFullSnapshot() {
      if (!isConnected() || _fullRequestCooldown > 0.0f) return;
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::SnapshotRequest, _outgoingSeq++,
        static_cast<std::uint16_t>(sizeof(_localNetId) + sizeof(_sessionToken))
      };
      network::PacketStreamWriter writer;
      writer << header << _localNetId << _sessionToken;
      _socket.send(writer.getBuffer(), _serverEndpoint);
      _fullRequestCooldown = FULL_REQUEST_INTERVAL;
      ++_fullSnapshotRequests;
    }

    const network::SnapshotState* findSnapshot(std::uint32_t tick) const {
      for (const auto& [storedTick, state] : _snapshotHistory)
        if (storedTick == tick) return &state;
      return nullptr;
    }

    void storeSnapshot(std::uint32_t tick, network::SnapshotState state) {
      for (auto& [storedTick, stored] : _snapshotHistory) {
        if (storedTick == tick) {
          stored = std::move(state);
          return;
        }
      }
      _snapshotHistory.emplace_back(tick, std::move(state));
      while (_snapshotHistory.size() > MAX_SNAPSHOT_HISTORY) {
        auto oldest = std::min_element(_snapshotHistory.begin(), _snapshotHistory.end(),
                                       [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
        _snapshotHistory.erase(oldest);
      }
    }

    static constexpr float CONNECT_RETRY_INTERVAL = 0.5f;
    static constexpr std::uint8_t MAX_CONNECT_RETRIES = 8;
    static constexpr std::size_t ROOM_ENTRY_SIZE = 8;
    static constexpr std::uint16_t ROOM_RESULT_SIZE = 5;
    static constexpr std::size_t MAX_RELIABLE_HISTORY = 512;
    static constexpr std::size_t MAX_DESTROYED_HISTORY = 1024;
    static constexpr std::size_t MAX_PENDING_PLAYER_SPAWNS = 4;

    static bool isReliableServerPacket(network::PacketType type) {
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

    static bool isSequenceNewer(std::uint32_t candidate,
                                std::uint32_t previous, bool initialized) {
      if (!initialized) return true;
      const std::uint32_t delta = candidate - previous;
      return delta != 0 && delta < 0x80000000u;
    }

    static std::uint64_t augmentKey(network::NetworkId netId,
                                    std::uint8_t augment) {
      return (static_cast<std::uint64_t>(netId) << 8) | augment;
    }

    void startConnecting(const network::PacketData& packet) {
      _roomListRequest.reset();
      _roomCreateRequest.reset();
      _joinRoomId.reset();
      _joinAccepted = false;
      _connectPacket = packet;
      _socket.send(_connectPacket, _serverEndpoint);
      _connectRetryTimer = CONNECT_RETRY_INTERVAL;
      _connectAttempts = 0;
      _state = SessionState::Connecting;
      _disconnectReason = DisconnectReason::None;
      _sessionToken = 0;
      _serverDifficulty.reset();
      _receivedReliable.clear();
      _receivedReliableOrder.clear();
      _pendingPlayerSpawns.clear();
      _pendingHealth.clear();
      _pendingAugments.clear();
      _lastHealthSequences.clear();
      _lastAugmentSequences.clear();
      _lastScoreSequences.clear();
      _hasSyncSequence = false;
      _hasSnapshotTick = false;
      _hasInputAck = false;
      _snapshotHistory.clear();
      _pendingDeltas.clear();
      _fullRequestCooldown = 0.0f;
      _awaitingFullSnapshot = false;
    }

    struct LobbyRequest {
      network::PacketData packet;
      float timer{0.0f};
      std::uint8_t attempts{0};
    };

    network::PacketData buildPacket(network::PacketType type, const network::PacketData& payload) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++, static_cast<std::uint16_t>(payload.size())
      };
      network::PacketStreamWriter writer;
      writer << header;
      network::PacketData packet = writer.getBuffer();
      packet.insert(packet.end(), payload.begin(), payload.end());
      return packet;
    }

    void startLobbyRequest(std::optional<LobbyRequest>& request, network::PacketType type,
                           const network::PacketData& payload) {
      request = LobbyRequest{buildPacket(type, payload), CONNECT_RETRY_INTERVAL, 0};
      _socket.send(request->packet, _serverEndpoint);
    }

    bool retryLobbyRequest(std::optional<LobbyRequest>& request, float dt) {
      if (!request) return false;
      request->timer -= std::max(dt, 0.0f);
      if (request->timer > 0.0f) return false;
      if (request->attempts >= MAX_CONNECT_RETRIES) {
        request.reset();
        return true;
      }
      _socket.send(request->packet, _serverEndpoint);
      ++request->attempts;
      request->timer = CONNECT_RETRY_INTERVAL;
      return false;
    }

    static RoomResult createResult(std::uint8_t status) {
      switch (status) {
        case 0: return RoomResult::Success;
        case 1: return RoomResult::InvalidSettings;
        case 2: return RoomResult::LimitReached;
        default: return RoomResult::Unknown;
      }
    }

    static RoomResult joinResult(std::uint8_t status) {
      switch (status) {
        case 0: return RoomResult::Success;
        case 1: return RoomResult::NotFound;
        case 2: return RoomResult::Full;
        case 3: return RoomResult::AlreadyInRoom;
        default: return RoomResult::Unknown;
      }
    }

    void handleRoomList(network::PacketStreamReader& reader) {
      if (!_roomListRequest) return;
      std::uint16_t count{0};
      reader >> count;
      if (reader.remaining() != static_cast<std::size_t>(count) * ROOM_ENTRY_SIZE) return;
      std::vector<RoomInfo> rooms(count);
      for (auto& room : rooms)
        reader >> room.id >> room.players >> room.capacity >> room.state >> room.difficulty;
      _roomListRequest.reset();
      if (_onRoomList) _onRoomList(RoomResult::Success, rooms);
    }

    void handleRoomCreate(network::PacketStreamReader& reader) {
      if (!_roomCreateRequest) return;
      std::uint32_t roomId{0};
      std::uint8_t status{0};
      reader >> roomId >> status;
      _roomCreateRequest.reset();
      if (_onRoomCreated) _onRoomCreated(createResult(status), roomId);
    }

    void handleRoomJoin(network::PacketStreamReader& reader) {
      std::uint32_t roomId{0};
      std::uint8_t status{0};
      reader >> roomId >> status;
      if (_state != SessionState::Connecting || !_joinRoomId || *_joinRoomId != roomId) return;
      const RoomResult result = joinResult(status);
      if (result == RoomResult::Success) {
        if (_joinAccepted) return;
        _joinAccepted = true;
      } else {
        _connectPacket.clear();
        _joinRoomId.reset();
        _state = SessionState::Lobby;
      }
      if (_onRoomJoined) _onRoomJoined(result, roomId);
    }

    void openSocket(const std::string& host, unsigned short port) {
      _serverEndpoint = {host, port};
      if (_socketOpen) return;
      _socket.bind(0);
      _socket.setNonBlocking(true);
      _socketOpen = true;
    }

    void endSession(DisconnectReason reason) {
      _roomListRequest.reset();
      _roomCreateRequest.reset();
      _joinRoomId.reset();
      _joinAccepted = false;
      _connectPacket.clear();
      _sessionToken = 0;
      _state = SessionState::Disconnected;
      _disconnectReason = reason;
    }

    void sendAcknowledgement(std::uint32_t acknowledgedSequence) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::Ack,
        _outgoingSeq++, sizeof(acknowledgedSequence)
      };
      network::PacketStreamWriter writer;
      writer << header << acknowledgedSequence;
      _socket.send(writer.getBuffer(), _serverEndpoint);
    }

    bool rememberReliableSequence(std::uint32_t sequence) {
      if (_receivedReliable.contains(sequence)) return false;
      _receivedReliable.insert(sequence);
      _receivedReliableOrder.push_back(sequence);
      if (_receivedReliableOrder.size() > MAX_RELIABLE_HISTORY) {
        _receivedReliable.erase(_receivedReliableOrder.front());
        _receivedReliableOrder.pop_front();
      }
      return true;
    }

    void spawnEntity(network::NetworkId netId, network::ext::EntityType type,
                     float x, float y, std::uint32_t sequence) {
      auto tombstone = _destroyedEntities.find(netId);
      if (tombstone != _destroyedEntities.end()) {
        if (!isSequenceNewer(sequence, tombstone->second, true)) return;
        _destroyedEntities.erase(tombstone);
      }
      if (_networkToEcs.find(netId) != _networkToEcs.end()) return;

      auto entity = _registry.createEntity();
      const bool isLocal = type == network::ext::ENTITY_PLAYER &&
                           netId == _localNetId;
      _registry.addComponent<engine::TransformComponent>(
        entity, {{x, y}, 0.0f, {1.0f, 1.0f}, {x, y}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{0.0f, 0.0f}});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {netId, isLocal});
      if (!isLocal) _registry.addComponent(entity, engine::SnapshotBufferComponent{{}, _snapshotBufferSize});
      _networkToEcs[netId] = RemoteEntity{entity, type};
      if (_onSpawn) _onSpawn(entity, netId, type, isLocal);

      auto health = _pendingHealth.find(netId);
      if (health != _pendingHealth.end()) {
        if (isSequenceNewer(health->second.sequence, sequence, true))
          updateHealth(netId, health->second.health, health->second.maximum,
                       health->second.sequence);
        _pendingHealth.erase(health);
      }
      for (std::uint8_t augmentId = 1; augmentId <= 3; ++augmentId) {
        const auto key = augmentKey(netId, augmentId);
        auto augment = _pendingAugments.find(key);
        if (augment == _pendingAugments.end()) continue;
        const auto last = _lastAugmentSequences.find(key);
        const bool isLatest = last == _lastAugmentSequences.end() ||
          isSequenceNewer(augment->second.sequence, last->second, true);
        if (isLatest && isSequenceNewer(augment->second.sequence, sequence,
                                        true) && _onAugment) {
          _lastAugmentSequences[key] = augment->second.sequence;
          _onAugment(entity, netId, type, augmentId,
                     augment->second.level, augment->second.remaining);
        }
        _pendingAugments.erase(augment);
      }
    }

    void destroyEntity(network::NetworkId netId, std::uint32_t sequence) {
      auto it = _networkToEcs.find(netId);
      if (it != _networkToEcs.end()) {
        const RemoteEntity removed = it->second;
        _networkToEcs.erase(it);
        if (shouldDeferDestroy(removed.entity))
          _pendingDestroys.push_back({removed.entity, netId, removed.type, _lastSnapshotTick + 1, 0.0f});
        else
          finishDestroy(removed.entity, netId, removed.type);
      }
      _playerSkins.erase(netId);
      _pendingHealth.erase(netId);
      _lastHealthSequences.erase(netId);
      for (auto pending = _pendingAugments.begin();
           pending != _pendingAugments.end();) {
        if (static_cast<network::NetworkId>(pending->first >> 8) == netId)
          pending = _pendingAugments.erase(pending);
        else
          ++pending;
      }
      for (auto last = _lastAugmentSequences.begin();
           last != _lastAugmentSequences.end();) {
        if (static_cast<network::NetworkId>(last->first >> 8) == netId)
          last = _lastAugmentSequences.erase(last);
        else
          ++last;
      }
      _destroyedEntities[netId] = sequence;
      if (_destroyedEntities.size() > MAX_DESTROYED_HISTORY)
        _destroyedEntities.erase(_destroyedEntities.begin());
    }

    void updateHealth(network::NetworkId netId, int health, int maxHealth,
                      std::uint32_t sequence) {
      auto it = _networkToEcs.find(netId);
      if (it == _networkToEcs.end()) {
        auto pending = _pendingHealth.find(netId);
        if (pending == _pendingHealth.end() ||
            isSequenceNewer(sequence, pending->second.sequence, true)) {
          _pendingHealth[netId] = PendingHealth{health, maxHealth, sequence};
        }
        return;
      }
      const auto last = _lastHealthSequences.find(netId);
      if (last != _lastHealthSequences.end() &&
          !isSequenceNewer(sequence, last->second, true)) return;
      _lastHealthSequences[netId] = sequence;
      const engine::Entity entity = it->second.entity;
      auto* component = _registry.getComponent<engine::HealthComponent>(entity);
      const int previous = component ? component->current : maxHealth;
      _registry.addComponent(entity, engine::HealthComponent{health, maxHealth});
      if (_onHealth)
        _onHealth(entity, netId, it->second.type, health, maxHealth, previous);
    }

    bool shouldDeferDestroy(engine::Entity entity) const {
      return _snapshotClock && _snapshotClock->isSynchronized() && _hasSnapshotTick &&
             _registry.getComponent<engine::SnapshotBufferComponent>(entity) != nullptr;
    }

    void finishDestroy(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type) {
      if (_onDestroy) _onDestroy(entity, netId, type);
      _registry.destroyEntity(entity);
    }

    void flushPendingDestroys(float dt) {
      if (_pendingDestroys.empty()) return;
      const bool synchronized = _snapshotClock && _snapshotClock->isSynchronized();
      const auto renderTick = _snapshotClock ? _snapshotClock->currentRenderTick() : std::nullopt;
      std::vector<PendingDestroy> ready;
      for (auto pending = _pendingDestroys.begin(); pending != _pendingDestroys.end();) {
        pending->waited += std::max(dt, 0.0f);
        const bool reached = renderTick && *renderTick >= static_cast<double>(pending->tick);
        if (!synchronized || reached || pending->waited >= MAX_DEFERRED_DESTROY_SECONDS) {
          ready.push_back(*pending);
          pending = _pendingDestroys.erase(pending);
        } else {
          ++pending;
        }
      }
      for (const auto& entry : ready) finishDestroy(entry.entity, entry.netId, entry.type);
    }

    void recordSnapshotSample(const network::EntityStateData& state, std::uint32_t tick) {
      auto it = _networkToEcs.find(state.netId);
      if (it == _networkToEcs.end()) return;
      auto* buffer = _registry.getComponent<engine::SnapshotBufferComponent>(it->second.entity);
      if (!buffer) return;
      engine::pushSnapshotSample(*buffer, {tick, {state.posX, state.posY}, {state.velX, state.velY}});
    }

    void updateEntityState(const network::EntityStateData& state) {
      auto it = _networkToEcs.find(state.netId);
      if (it == _networkToEcs.end()) return;
      auto* transform = _registry.getComponent<engine::TransformComponent>(
        it->second.entity);
      if (transform) transform->targetPosition = {state.posX, state.posY};
    }

    network::INetworkSocket& _socket;
    engine::Registry& _registry;
    SpawnCallback _onSpawn;
    DestroyCallback _onDestroy;
    HealthCallback _onHealth;
    AugmentCallback _onAugment;
    ScoreCallback _onScore;
    InputAckCallback _onInputAck;
    RoomListCallback _onRoomList;
    RoomCallback _onRoomCreated;
    RoomCallback _onRoomJoined;
    std::optional<LobbyRequest> _roomListRequest;
    std::optional<LobbyRequest> _roomCreateRequest;
    std::optional<std::uint32_t> _joinRoomId;
    bool _joinAccepted{false};
    std::uint32_t _lastInputAck{0};
    bool _hasInputAck{false};
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, RemoteEntity> _networkToEcs;
    std::unordered_map<network::NetworkId, PendingHealth> _pendingHealth;
    std::unordered_map<std::uint64_t, PendingAugment> _pendingAugments;
    std::unordered_map<network::NetworkId, std::uint32_t> _lastHealthSequences;
    std::unordered_map<std::uint64_t, std::uint32_t> _lastAugmentSequences;
    std::unordered_map<network::NetworkId, std::uint32_t> _lastScoreSequences;
    std::unordered_map<network::NetworkId, std::uint32_t> _destroyedEntities;
    std::vector<PendingPlayerSpawn> _pendingPlayerSpawns;

    network::NetworkId _localNetId{0};
    std::uint64_t _sessionToken{0};
    network::PacketData _connectPacket;
    float _connectRetryTimer{0.0f};
    std::uint8_t _connectAttempts{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastSyncSeq{0};
    bool _hasSyncSequence{false};
    std::uint32_t _lastSnapshotTick{0};
    bool _hasSnapshotTick{false};
    std::size_t _snapshotBufferSize{32};
    engine::SnapshotClock* _snapshotClock{nullptr};
    std::vector<PendingDestroy> _pendingDestroys;
    std::size_t _lateSnapshots{0};
    std::size_t _unknownBaseDeltas{0};
    std::size_t _fullSnapshotRequests{0};
    std::size_t _fullSnapshotsReceived{0};
    std::size_t _deltaSnapshotsReceived{0};
    std::size_t _snapshotBytesReceived{0};
    float _fullRequestCooldown{0.0f};
    bool _awaitingFullSnapshot{false};
    std::vector<PendingDelta> _pendingDeltas;
    std::deque<std::pair<std::uint32_t, network::SnapshotState>> _snapshotHistory;
    std::unordered_set<std::uint32_t> _receivedReliable;
    std::deque<std::uint32_t> _receivedReliableOrder;
    std::size_t _droppedPackets{0};
    std::optional<std::uint8_t> _serverDifficulty;
    std::unordered_map<network::NetworkId, std::uint8_t> _playerSkins;
    bool _socketOpen{false};
    SessionState _state{SessionState::Disconnected};
    DisconnectReason _disconnectReason{DisconnectReason::None};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
