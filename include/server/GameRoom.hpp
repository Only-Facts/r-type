#ifndef GAMEROOM_HPP
#define GAMEROOM_HPP

#include "gameplay/PlayerMovement.hpp"
#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include "network/SnapshotDelta.hpp"
#include "ReliableUdpQueue.hpp"
#include "ServerMetrics.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <deque>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <random>
#include <limits>
#include <cstring>
#include <utility>

namespace rtype::server {

  enum class ServerEntityType : std::uint8_t {
    Player = 0,
    Bydos = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
    BonusDouble = 4,
    BonusTriple = 5,
    BonusHeal = 6,
    BonusDamage = 7,
    PlayerMissileHeavy = 8,
    Boss = 9
  };

  struct ServerEntity {
    network::NetworkId id;
    ServerEntityType type;
    float posX{0.f};
    float posY{0.f};
    float velX{0.f};
    float velY{0.f};
    int hp{1};
    int maxHp{1};
    network::NetworkId ownerId{0};
    int damage{1};
    float age{0.0f};
    float baseY{0.0f};
  };

  struct QueuedInput {
    std::uint32_t sequence{0};
    gameplay::MovementInput movement;
    bool shoot{false};
  };

  struct ConnectedClient {
    network::NetworkId id;
    network::Endpoint endpoint;
    float posX{200.0f};
    float posY{300.0f};
    float velX{0.0f};
    float velY{0.0f};
    float idleTime{0.0f};
    std::uint8_t difficulty{1};
    std::uint8_t skin{0xFF};
    int hp{5};
    int maxHp{5};
    bool dead{false};
    float deathTimer{0.f};
    float shootCooldown{0.f};
    bool shootPressed{false};
    float hitCooldown{0.0f};
    gameplay::MovementInput movement;
    std::deque<QueuedInput> pendingInputs;
    std::uint32_t lastProcessedInput{0};
    bool hasProcessedInput{false};
    bool needsFullSnapshot{true};
    std::uint32_t score{0};
    std::uint64_t sessionToken{0};
    std::uint32_t lastInputSequence{0};
    bool hasInputSequence{false};
    std::uint8_t shotPattern{0};
    float shotPatternRemaining{0.0f};
    std::uint8_t damageLevel{0};
    float damageRemaining{0.0f};
  };

  class GameRoom {
  public:
    using RoomId = std::uint32_t;

    explicit GameRoom(network::INetworkSocket& socket, RoomId id,
                      std::size_t maxPlayers, std::uint8_t difficulty,
                      bool fixedDifficulty, std::uint32_t& outgoingSequence,
                      ReliableUdpQueue* reliableQueue = nullptr,
                      ServerMetrics* metrics = nullptr)
      : _socket(socket), _roomId(id), _maxPlayers(maxPlayers),
        _fixedDifficulty(fixedDifficulty), _outgoingSeq(outgoingSequence),
        _reliableQueue(reliableQueue), _metrics(metrics),
        _currentDifficulty(difficulty),
        _rng(std::random_device{}() ^ (id * 0x9E3779B9u)) {}

    void update(float dt) {
      const float elapsed = std::isfinite(dt) ? std::max(dt, 0.0f) : 0.0f;
      removeTimedOutClients(elapsed);
      if (_clients.empty()) return;
      const float accepted = std::min(elapsed, MAX_FRAME_DELTA);
      _droppedSimulationSeconds += static_cast<double>(elapsed - accepted);
      _tickAccumulator += accepted;
      std::size_t steps = 0;
      while (_tickAccumulator >= FIXED_TICK && steps < MAX_CATCHUP_TICKS) {
        updateGameLogic(FIXED_TICK);
        _tickAccumulator -= FIXED_TICK;
        ++steps;
        if (_clients.empty()) return;
      }
      if (steps == MAX_CATCHUP_TICKS && _tickAccumulator >= FIXED_TICK) {
        const float retained = std::fmod(_tickAccumulator, FIXED_TICK);
        _droppedSimulationSeconds += static_cast<double>(
          _tickAccumulator - retained);
        _tickAccumulator = retained;
      }
      broadcastGameState();
    }

    void handlePacket(const network::PacketData& data,
                      const network::Endpoint& sender) {
      if (data.size() < sizeof(network::PacketHeader)) return;

      try {
        network::PacketStreamReader reader(data);
        network::PacketHeader header{};
        reader >> header;
        if (header.magic != network::PROTOCOL_MAGIC) return;
        if (header.payloadSize != data.size() - sizeof(header)) return;
        switch (header.type) {
          case network::PacketType::ConnectReq:
            handleConnect(reader, sender, header.payloadSize);
            break;
          case network::PacketType::PlayerInput:
            if (header.payloadSize == sizeof(network::NetworkId) +
                sizeof(std::uint64_t) + 5)
              handleInput(reader, sender, header.sequence);
            break;
          case network::PacketType::Disconnect:
            if (header.payloadSize == sizeof(network::NetworkId) +
                sizeof(std::uint64_t))
              handleDisconnect(reader, sender);
            break;
          case network::PacketType::SnapshotRequest:
            if (header.payloadSize == sizeof(network::NetworkId) +
                sizeof(std::uint64_t))
              handleSnapshotRequest(reader, sender);
            break;
          default:
            break;
        }
      } catch (const RtypeError&) {
        return;
      }
    }

    [[nodiscard]] RoomId id() const { return _roomId; }
    [[nodiscard]] std::size_t playerCount() const { return _clients.size(); }
    [[nodiscard]] std::size_t maxPlayers() const { return _maxPlayers; }
    [[nodiscard]] std::uint8_t difficulty() const { return _currentDifficulty; }
    [[nodiscard]] bool isFull() const { return playerCount() >= _maxPlayers; }
    [[nodiscard]] bool isEmpty() const { return _clients.empty(); }
    [[nodiscard]] bool bossActive() const { return _bossActive; }
    [[nodiscard]] double droppedSimulationSeconds() const {
      return _droppedSimulationSeconds;
    }
    [[nodiscard]] std::uint32_t stageNumber() const { return _stageNumber; }
    [[nodiscard]] std::uint32_t killsThisStage() const { return _killsThisStage; }
    [[nodiscard]] std::uint32_t scoreFor(network::NetworkId id) const {
      const auto it = _clients.find(id);
      return it == _clients.end() ? 0 : it->second.score;
    }
    [[nodiscard]] std::size_t entityCount(ServerEntityType type) const {
      return static_cast<std::size_t>(std::count_if(
        _entities.begin(), _entities.end(), [type](const auto& entry) {
          return entry.second.type == type;
        }));
    }
    [[nodiscard]] std::uint8_t augmentLevel(network::NetworkId id,
                                            std::uint8_t augment) const {
      const auto it = _clients.find(id);
      if (it == _clients.end()) return 0;
      if (augment == 1 || augment == 2)
        return it->second.shotPattern == augment ? 1 : 0;
      return augment == 3 ? it->second.damageLevel : 0;
    }

    [[nodiscard]] bool containsEndpoint(const network::Endpoint& endpoint) const {
      for (const auto& [id, client] : _clients) {
        if (sameEndpoint(client.endpoint, endpoint)) return true;
      }
      return false;
    }

    [[nodiscard]] bool canAccept(const network::Endpoint& endpoint) const {
      return containsEndpoint(endpoint) || !isFull();
    }

    [[nodiscard]] bool containsClient(network::NetworkId id,
                                      const network::Endpoint& endpoint) const {
      auto it = _clients.find(id);
      return it != _clients.end() && sameEndpoint(it->second.endpoint, endpoint);
    }

    [[nodiscard]] bool validateSession(network::NetworkId id,
                                       const network::Endpoint& endpoint,
                                       std::uint64_t token) const {
      const auto it = _clients.find(id);
      return it != _clients.end() && token != 0 &&
        it->second.sessionToken == token &&
        sameEndpoint(it->second.endpoint, endpoint);
    }

  private:
    friend struct GameRoomTestAccess;

    static constexpr std::uint32_t BYDOS_KILLS_PER_BOSS = 10;
    static constexpr float AUGMENT_DURATION = 10.0f;
    static constexpr float PLAYER_MISSILE_SPEED = 900.0f;
    static constexpr float FIXED_TICK = 1.0f / 60.0f;
    static constexpr float WORLD_WIDTH = 1920.0f;
    static constexpr float WORLD_HEIGHT = 1080.0f;
    static constexpr float PLAYER_WIDTH = 33.0f;
    static constexpr float PLAYER_HEIGHT = 34.0f;
    static constexpr float BOSS_WIDTH = 224.0f;
    static constexpr float BOSS_HEIGHT = 224.0f;
    static constexpr float PLAYER_MISSILE_WIDTH = 18.0f;
    static constexpr float PLAYER_MISSILE_HEAVY_WIDTH = 24.0f;
    static constexpr float PLAYER_MISSILE_HEIGHT = 6.0f;
    static constexpr float MAX_FRAME_DELTA = 0.25f;
    static constexpr std::size_t MAX_CATCHUP_TICKS = 15;
    static constexpr std::size_t MAX_ACTIVE_ENTITIES = 48;
    static constexpr std::size_t MAX_SNAPSHOT_ENTITIES = 52;
    static constexpr std::uint64_t SNAPSHOT_KEYFRAME_INTERVAL = 60;
    static constexpr std::size_t MAX_PENDING_INPUTS = 8;
    static constexpr std::size_t MAX_INPUTS_PER_TICK = 2;

    static bool rectanglesOverlap(float ax, float ay, float aw, float ah,
                                  float bx, float by, float bw, float bh) {
      return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
    }

    static bool missileHitsBoss(const ServerEntity& missile, const ServerEntity& boss) {
      const float width = missile.type == ServerEntityType::PlayerMissileHeavy
        ? PLAYER_MISSILE_HEAVY_WIDTH : PLAYER_MISSILE_WIDTH;
      return rectanglesOverlap(missile.posX, missile.posY, width, PLAYER_MISSILE_HEIGHT,
                               boss.posX, boss.posY, BOSS_WIDTH, BOSS_HEIGHT);
    }

    static bool sameEndpoint(const network::Endpoint& lhs,
                             const network::Endpoint& rhs) {
      return lhs.address == rhs.address && lhs.port == rhs.port;
    }
    static constexpr float CLIENT_TIMEOUT = 3.0f;

    void handleConnect(network::PacketStreamReader& reader,
                       const network::Endpoint& sender,
                       std::uint16_t payloadSize) {
      if (payloadSize != 0 && payloadSize != 1 &&
          payloadSize != 2 && payloadSize != 6) return;

      for (auto& [id, existing] : _clients) {
        if (sameEndpoint(existing.endpoint, sender)) {
          sendConnectAck(existing);
          resendWorldToClient(existing);
          return;
        }
      }

      std::uint8_t diff = 1;
      std::uint8_t skin = 0xFF;
      network::NetworkId requestedPrevId = 0;

      if (payloadSize >= 1) reader >> diff;
      if (payloadSize >= 2) reader >> skin;
      if (payloadSize == 6) reader >> requestedPrevId;

      if (diff != 0xFF && diff > 2) diff = 1;
      if (!_fixedDifficulty && !_clients.empty()) {
        diff = _currentDifficulty;
      } else if (!_fixedDifficulty) {
        _currentDifficulty = (diff == 0xFF ? 1 : diff);
      }
      if (_clients.size() >= _maxPlayers) return;

      network::NetworkId netId = 0;
      if (requestedPrevId >= 1000 && requestedPrevId < 2000 &&
          _clients.find(requestedPrevId) == _clients.end()) {
        netId = requestedPrevId;
      } else {
        netId = allocatePlayerId();
      }
      if (netId == 0) return;

      int startHp = 5;
      if (_currentDifficulty == 0) startHp = 8;
      else if (_currentDifficulty == 2) startHp = 4;

      const float startX = 200.0f + ((netId % 4) * 50.0f);
      const float startY = 300.0f + ((netId % 4) * 40.0f);
      ConnectedClient client;
      client.id = netId;
      client.endpoint = sender;
      client.posX = startX;
      client.posY = startY;
      client.difficulty = _currentDifficulty;
      client.skin = skin;
      client.hp = startHp;
      client.maxHp = startHp;
      client.sessionToken = generateSessionToken();
      _clients[netId] = client;
      const ConnectedClient& connected = _clients.at(netId);

      sendConnectAck(connected);
      sendSpawnPlayer(connected, sender);
      sendHealthUpdate(connected.id, connected.hp, connected.maxHp, sender);

      for (const auto& [id, existing] : _clients) {
        if (id == netId) continue;
        sendSpawnPlayer(existing, sender);
        sendHealthUpdate(existing.id, existing.hp, existing.maxHp, sender);
      }

      for (const auto& [id, existing] : _clients) {
        sendSpawnPlayer(connected, existing.endpoint);
        sendHealthUpdate(netId, connected.hp, connected.maxHp, existing.endpoint);
      }
      for (const auto& [id, existing] : _clients) {
        sendScoreUpdate(existing, sender);
        sendActiveAugments(existing, sender);
        if (id != netId) sendScoreUpdate(connected, existing.endpoint);
      }
      sendScoreUpdate(connected, sender);

      for (const auto& [entId, ent] : _entities) {
        sendSpawnEntity(ent, sender);
        if (ent.maxHp > 1) {
          sendHealthUpdate(entId, ent.hp, ent.maxHp, sender);
        }
      }
    }

    network::NetworkId allocatePlayerId() {
      for (std::uint32_t attempt = 0; attempt < 1000; ++attempt) {
        const network::NetworkId candidate = _nextPlayerId++;
        if (_nextPlayerId >= 2000) _nextPlayerId = 1000;
        if (_clients.find(candidate) == _clients.end()) return candidate;
      }
      return 0;
    }

    void sendConnectAck(const ConnectedClient& client) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::ConnectAck,
        _outgoingSeq++, 13
      };
      network::PacketStreamWriter writer;
      writer << header << client.id << client.difficulty << client.sessionToken;
      sendDatagram(writer.getBuffer(), client.endpoint);
    }

    void resendWorldToClient(const ConnectedClient& client) {
      sendSpawnPlayer(client, client.endpoint);
      sendHealthUpdate(client.id, client.hp, client.maxHp, client.endpoint);
      for (const auto& [id, existing] : _clients) {
        if (id == client.id) continue;
        sendSpawnPlayer(existing, client.endpoint);
        sendHealthUpdate(existing.id, existing.hp, existing.maxHp, client.endpoint);
        sendScoreUpdate(existing, client.endpoint);
        sendActiveAugments(existing, client.endpoint);
      }
      sendScoreUpdate(client, client.endpoint);
      sendActiveAugments(client, client.endpoint);
      for (const auto& [entId, ent] : _entities) {
        sendSpawnEntity(ent, client.endpoint);
        if (ent.maxHp > 1) sendHealthUpdate(entId, ent.hp, ent.maxHp, client.endpoint);
      }
    }

    void sendSpawnPlayer(const ConnectedClient& target, const network::Endpoint& recipient) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::SpawnPlayer,
        _outgoingSeq++, 13
      };
      network::PacketStreamWriter writer;
      writer << header << target.id << target.posX << target.posY << target.skin;
      sendDatagram(writer.getBuffer(), recipient);
    }

    void sendSpawnEntity(const ServerEntity& ent, const network::Endpoint& recipient) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::SpawnEntity,
        _outgoingSeq++, 13
      };
      network::PacketStreamWriter writer;
      writer << header << ent.id << static_cast<std::uint8_t>(ent.type) << ent.posX << ent.posY;
      sendDatagram(writer.getBuffer(), recipient);
    }

    void sendHealthUpdate(network::NetworkId netId, std::uint8_t hp,
                          std::uint8_t maxHp,
                          const network::Endpoint& recipient) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::HealthUpdate,
        _outgoingSeq++, 6
      };
      network::PacketStreamWriter writer;
      writer << header << netId << hp << maxHp;
      sendDatagram(writer.getBuffer(), recipient);
    }

    void handleInput(network::PacketStreamReader& reader,
                     const network::Endpoint& sender,
                     std::uint32_t sequence) {
      network::NetworkId netId{};
      std::uint64_t sessionToken{};
      std::uint8_t upValue{}, downValue{}, leftValue{};
      std::uint8_t rightValue{}, shootValue{};
      reader >> netId >> sessionToken >> upValue >> downValue >> leftValue
             >> rightValue >> shootValue;
      if (upValue > 1 || downValue > 1 || leftValue > 1 ||
          rightValue > 1 || shootValue > 1) {
        if (_metrics)
          _metrics->malformedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      const bool up = upValue != 0;
      const bool down = downValue != 0;
      const bool left = leftValue != 0;
      const bool right = rightValue != 0;
      const bool shoot = shootValue != 0;

      auto it = _clients.find(netId);
      if (it == _clients.end() ||
          !sameEndpoint(it->second.endpoint, sender) ||
          it->second.sessionToken != sessionToken ||
          !isNewerSequence(sequence, it->second.lastInputSequence,
                           it->second.hasInputSequence)) {
        if (_metrics)
          _metrics->rejectedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      it->second.lastInputSequence = sequence;
      it->second.hasInputSequence = true;
      it->second.idleTime = 0.0f;

      auto& queue = it->second.pendingInputs;
      if (queue.size() >= MAX_PENDING_INPUTS) queue.pop_front();
      queue.push_back({sequence, gameplay::MovementInput{up, down, left, right}, shoot});
    }

    static void processQueuedInputs(ConnectedClient& client) {
      std::size_t processed = 0;
      while (!client.pendingInputs.empty() && processed < MAX_INPUTS_PER_TICK) {
        const QueuedInput input = client.pendingInputs.front();
        client.pendingInputs.pop_front();
        client.movement = input.movement;
        client.shootPressed = input.shoot;
        const gameplay::MovementVector moved = gameplay::applyMovement(
          {client.posX, client.posY}, input.movement, gameplay::INPUT_STEP_SECONDS, movementBounds());
        client.posX = moved.x;
        client.posY = moved.y;
        const gameplay::MovementVector velocity = gameplay::movementVelocity(input.movement);
        client.velX = velocity.x;
        client.velY = velocity.y;
        client.lastProcessedInput = input.sequence;
        client.hasProcessedInput = true;
        ++processed;
      }
    }

    static void discardQueuedInputs(ConnectedClient& client) {
      if (!client.pendingInputs.empty()) {
        client.lastProcessedInput = client.pendingInputs.back().sequence;
        client.hasProcessedInput = true;
        client.pendingInputs.clear();
      }
      client.movement = {};
      client.shootPressed = false;
      client.velX = 0.0f;
      client.velY = 0.0f;
    }

    void sendInputAcks() {
      for (const auto& [id, client] : _clients) {
        if (!client.hasProcessedInput) continue;
        network::PacketStreamWriter payload;
        payload << client.lastProcessedInput << _simulationTick << client.posX << client.posY;
        network::PacketHeader header{
          network::PROTOCOL_MAGIC, network::PacketType::InputAck, _outgoingSeq++,
          static_cast<std::uint16_t>(payload.getBuffer().size())
        };
        network::PacketStreamWriter writer;
        writer << header;
        auto packet = writer.getBuffer();
        packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());
        sendDatagram(packet, client.endpoint, false);
      }
    }

    static gameplay::MovementBounds movementBounds() {
      return gameplay::MovementBounds{WORLD_WIDTH, WORLD_HEIGHT, PLAYER_WIDTH, PLAYER_HEIGHT};
    }

    void handleDisconnect(network::PacketStreamReader& reader,
                          const network::Endpoint& sender) {
      network::NetworkId netId{};
      std::uint64_t sessionToken{};
      reader >> netId >> sessionToken;
      auto it = _clients.find(netId);
      if (it == _clients.end() || !sameEndpoint(it->second.endpoint, sender) ||
          it->second.sessionToken != sessionToken) {
        if (_metrics)
          _metrics->rejectedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      removeClient(netId);
    }

    void handleSnapshotRequest(network::PacketStreamReader& reader,
                               const network::Endpoint& sender) {
      network::NetworkId netId{};
      std::uint64_t sessionToken{};
      reader >> netId >> sessionToken;
      auto it = _clients.find(netId);
      if (it == _clients.end() || !sameEndpoint(it->second.endpoint, sender) ||
          it->second.sessionToken != sessionToken) {
        if (_metrics)
          _metrics->rejectedPackets.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      it->second.needsFullSnapshot = true;
      if (_metrics) _metrics->fullSnapshotRequests.fetch_add(1, std::memory_order_relaxed);
    }

    void removeClient(network::NetworkId netId) {
      auto it = _clients.find(netId);
      if (it == _clients.end()) return;
      if (_reliableQueue) _reliableQueue->cancelEndpoint(it->second.endpoint);
      _clients.erase(it);
      removeOwnedProjectiles(netId);
      if (_clients.empty()) resetEmptyGame();
      broadcastDestroyEntity(netId);
    }

    void removeOwnedProjectiles(network::NetworkId ownerId) {
      std::vector<network::NetworkId> projectiles;
      for (const auto& [id, entity] : _entities) {
        if (isPlayerMissile(entity.type) && entity.ownerId == ownerId)
          projectiles.push_back(id);
      }
      for (const auto id : projectiles) {
        _entities.erase(id);
        broadcastDestroyEntity(id);
      }
    }

    void resetEmptyGame() {
      _entities.clear();
      _nextEntityId = 2000;
      _bydoSpawnTimer = 0.0f;
      _enemyShootTimer = 0.0f;
      _bossShootTimer = 0.0f;
      _stageNumber = 1;
      _killsThisStage = 0;
      _bossActive = false;
      _tickAccumulator = 0.0f;
      _snapshotTick = 0;
      _simulationTick = 0;
      _forceFullSnapshot = true;
      _previousSnapshot.clear();
      _hasPreviousSnapshot = false;
    }

    void removeTimedOutClients(float dt) {
      std::vector<network::NetworkId> dead;
      for (auto& [id, client] : _clients) {
        client.idleTime += dt;
        if (client.idleTime > CLIENT_TIMEOUT) dead.push_back(id);
      }
      for (auto id : dead) removeClient(id);
    }

    void updateGameLogic(float dt) {
      ++_simulationTick;
      std::vector<network::NetworkId> clientsToRemove;
      for (auto& [id, client] : _clients) {
        updateAugmentTimers(client, dt);
        client.hitCooldown = std::max(0.0f, client.hitCooldown - dt);
        if (client.dead) {
          discardQueuedInputs(client);
          client.deathTimer -= dt;
          if (client.deathTimer <= 0.f) clientsToRemove.push_back(id);
          continue;
        }
        processQueuedInputs(client);
        client.shootCooldown = std::max(0.0f, client.shootCooldown - dt);
        if (client.shootPressed && client.shootCooldown <= 0.f) {
          client.shootCooldown = 0.25f;
          spawnPlayerMissile(client);
        }
      }
      for (const auto id : clientsToRemove) removeClient(id);
      if (_clients.empty()) return;

      updateEnemySpawning(dt);
      updateEntities(dt);
      updateEnemyShooting(dt);

      std::vector<network::NetworkId> destroyedEntities;
      checkCollisions(destroyedEntities);
      sortUnique(destroyedEntities);
      for (const auto id : destroyedEntities) destroyEntity(id);
      maybeSpawnBoss();
    }

    void updateAugmentTimers(ConnectedClient& client, float dt) {
      if (client.shotPattern != 0) {
        client.shotPatternRemaining -= dt;
        if (client.shotPatternRemaining <= 0.0f) {
          sendAugmentUpdate(client.id, client.shotPattern, 0, 0.0f);
          client.shotPattern = 0;
          client.shotPatternRemaining = 0.0f;
        }
      }
      if (client.damageLevel > 0) {
        client.damageRemaining -= dt;
        if (client.damageRemaining <= 0.0f) {
          sendAugmentUpdate(client.id, 3, 0, 0.0f);
          client.damageLevel = 0;
          client.damageRemaining = 0.0f;
        }
      }
    }

    void updateEnemySpawning(float dt) {
      if (_bossActive || _killsThisStage >= BYDOS_KILLS_PER_BOSS) return;
      float interval = 1.2f;
      if (_currentDifficulty == 0) interval = 1.6f;
      else if (_currentDifficulty == 2) interval = 0.8f;
      interval = std::max(0.45f, interval *
        std::pow(0.94f, static_cast<float>(_stageNumber - 1)));
      _bydoSpawnTimer += dt;
      if (_bydoSpawnTimer >= interval &&
          entityCount(ServerEntityType::Bydos) < MAX_ACTIVE_BYDOS) {
        _bydoSpawnTimer = 0.0f;
        spawnBydo();
      }
    }

    void updateEntities(float dt) {
      std::vector<network::NetworkId> destroyed;
      for (auto& [id, entity] : _entities) {
        entity.age += dt;
        if (entity.type == ServerEntityType::Boss) {
          entity.posX = std::max(1510.0f, entity.posX + entity.velX * dt);
          entity.posY = entity.baseY + std::sin(entity.age * 1.35f) * 125.0f;
          continue;
        }
        entity.posX += entity.velX * dt;
        entity.posY += entity.velY * dt;
        if (entity.posX < -100.0f || entity.posX > 2120.0f ||
            entity.posY < -100.0f || entity.posY > 1180.0f) {
          destroyed.push_back(id);
        }
      }
      for (const auto id : destroyed) _entities.erase(id);
      for (const auto id : destroyed) broadcastDestroyEntity(id);
    }

    void updateEnemyShooting(float dt) {
      float interval = _currentDifficulty == 0 ? 4.0f :
        (_currentDifficulty == 1 ? 2.0f : 1.11f);
      interval = std::max(0.45f, interval /
        (1.0f + 0.08f * static_cast<float>(_stageNumber - 1)));
      _enemyShootTimer += dt;
      if (_enemyShootTimer >= interval) {
        _enemyShootTimer = 0.0f;
        std::vector<ServerEntity> shooters;
        for (const auto& [id, entity] : _entities) {
          if (entity.type == ServerEntityType::Bydos)
            shooters.push_back(entity);
        }
        for (const auto& shooter : shooters) spawnEnemyMissile(shooter);
      }

      if (!_bossActive) return;
      _bossShootTimer += dt;
      if (_bossShootTimer < 1.0f) return;
      _bossShootTimer = 0.0f;
      const auto boss = std::find_if(_entities.begin(), _entities.end(),
        [](const auto& entry) {
          return entry.second.type == ServerEntityType::Boss;
        });
      if (boss != _entities.end()) {
        const ServerEntity bossState = boss->second;
        spawnBossVolley(bossState);
      }
    }

    void spawnPlayerMissile(const ConnectedClient& client) {
      const auto type = client.damageLevel > 0
        ? ServerEntityType::PlayerMissileHeavy
        : ServerEntityType::PlayerMissile;
      const int damage = 1 + static_cast<int>(client.damageLevel);
      if (client.shotPattern == 1) {
        spawnMissile(client, type, damage, 0.0f, -8.0f);
        spawnMissile(client, type, damage, 0.0f, 8.0f);
      } else if (client.shotPattern == 2) {
        spawnMissile(client, type, damage, -8.0f);
        spawnMissile(client, type, damage, 0.0f);
        spawnMissile(client, type, damage, 8.0f);
      } else {
        spawnMissile(client, type, damage, 0.0f);
      }
    }

    bool canSpawnEntity() const {
      return _entities.size() < MAX_ACTIVE_ENTITIES;
    }

    network::NetworkId allocateEntityId() {
      for (std::size_t attempt = 0; attempt < MAX_ACTIVE_ENTITIES + 1; ++attempt) {
        if (_nextEntityId < 2000) _nextEntityId = 2000;
        const network::NetworkId candidate = _nextEntityId++;
        if (_entities.find(candidate) == _entities.end()) return candidate;
      }
      return 0;
    }

    void spawnMissile(const ConnectedClient& client, ServerEntityType type,
                      int damage, float angleDegrees, float offsetY = 0.0f) {
      if (!canSpawnEntity()) return;
      const float angle = angleDegrees * 0.017453292519943295f;
      ServerEntity missile;
      missile.id = allocateEntityId();
      if (missile.id == 0) return;
      missile.type = type;
      missile.posX = client.posX + 33.0f;
      missile.posY = client.posY + 6.0f + offsetY;
      missile.velX = PLAYER_MISSILE_SPEED * std::cos(angle);
      missile.velY = PLAYER_MISSILE_SPEED * std::sin(angle);
      missile.hp = 1;
      missile.maxHp = 1;
      missile.ownerId = client.id;
      missile.damage = damage;
      _entities[missile.id] = missile;
      broadcastSpawnEntity(missile);
    }

    void spawnBydo() {
      if (!canSpawnEntity()) return;
      ServerEntity bydo;
      bydo.id = allocateEntityId();
      if (bydo.id == 0) return;
      bydo.type = ServerEntityType::Bydos;
      bydo.posX = 1970.0f;
      bydo.posY = static_cast<float>(randomInt(80, 960));
      const float stageSpeed = std::min(
        1.0f + 0.035f * static_cast<float>(_stageNumber - 1), 1.65f);
      bydo.velX = -160.0f * stageSpeed;
      int hp = _currentDifficulty == 0 ? 1 :
        (_currentDifficulty == 2 ? 3 : 2);
      hp = std::min(8, hp + static_cast<int>(_stageNumber - 1) / 3);
      bydo.hp = hp;
      bydo.maxHp = hp;
      _entities[bydo.id] = bydo;
      broadcastSpawnEntity(bydo);
      if (bydo.maxHp > 1) broadcastHealthUpdate(bydo.id, bydo.hp, bydo.maxHp);
    }

    void spawnBoss() {
      std::vector<network::NetworkId> remainingEnemies;
      for (const auto& [id, entity] : _entities) {
        if (entity.type == ServerEntityType::Bydos ||
            entity.type == ServerEntityType::EnemyMissile)
          remainingEnemies.push_back(id);
      }
      for (const auto id : remainingEnemies) {
        _entities.erase(id);
        broadcastDestroyEntity(id);
      }
      if (!canSpawnEntity()) return;
      ServerEntity boss;
      boss.id = allocateEntityId();
      if (boss.id == 0) return;
      boss.type = ServerEntityType::Boss;
      boss.posX = 1810.0f;
      boss.posY = 540.0f;
      boss.baseY = boss.posY;
      boss.velX = -35.0f;
      const int playerScale = static_cast<int>(_clients.size()) * 8;
      const auto boundedStage = std::min<std::uint32_t>(_stageNumber, 12);
      const int stageScale = static_cast<int>(boundedStage) * 12;
      const int difficultyScale = static_cast<int>(_currentDifficulty) * 12;
      boss.hp = std::clamp(45 + playerScale + stageScale + difficultyScale,
                           1, 255);
      boss.maxHp = boss.hp;
      _entities[boss.id] = boss;
      _bossActive = true;
      _bossShootTimer = 0.0f;
      broadcastSpawnEntity(boss);
      broadcastHealthUpdate(boss.id, boss.hp, boss.maxHp);
    }

    void maybeSpawnBoss() {
      if (_bossActive || _killsThisStage < BYDOS_KILLS_PER_BOSS) return;
      spawnBoss();
    }

    void onBossDefeated() {
      _bossActive = false;
      if (_stageNumber < std::numeric_limits<std::uint32_t>::max())
        ++_stageNumber;
      _killsThisStage = 0;
      _bydoSpawnTimer = 0.0f;
      _enemyShootTimer = 0.0f;
      _bossShootTimer = 0.0f;
    }

    void spawnEnemyMissile(const ServerEntity& enemy) {
      if (!canSpawnEntity()) return;
      ServerEntity missile;
      missile.id = allocateEntityId();
      if (missile.id == 0) return;
      missile.type = ServerEntityType::EnemyMissile;
      missile.posX = enemy.posX - 10.0f;
      missile.posY = enemy.posY + 14.0f;
      float speed = _currentDifficulty == 0 ? 320.0f :
        (_currentDifficulty == 2 ? 520.0f : 420.0f);
      missile.velX = -speed;
      if (_currentDifficulty == 2) aimEnemyMissile(missile, enemy, speed);
      missile.hp = 1;
      missile.maxHp = 1;
      _entities[missile.id] = missile;
      broadcastSpawnEntity(missile);
    }

    void aimEnemyMissile(ServerEntity& missile, const ServerEntity& enemy,
                         float speed) const {
      float closestDistance = std::numeric_limits<float>::max();
      const ConnectedClient* target = nullptr;
      for (const auto& [id, client] : _clients) {
        if (client.dead || client.posX >= enemy.posX - 40.0f) continue;
        const float distance = std::hypot(client.posX - enemy.posX,
                                          client.posY - enemy.posY);
        if (distance < closestDistance) {
          closestDistance = distance;
          target = &client;
        }
      }
      if (!target) return;
      const float angle = std::clamp(
        std::atan2(target->posY - enemy.posY, enemy.posX - target->posX),
        -0.52f, 0.52f);
      missile.velX = -speed * std::cos(angle);
      missile.velY = speed * std::sin(angle);
    }

    void spawnBossVolley(const ServerEntity& boss) {
      for (const float angleDegrees : {-12.0f, 0.0f, 12.0f}) {
        if (!canSpawnEntity()) break;
        ServerEntity missile;
        missile.id = allocateEntityId();
        if (missile.id == 0) break;
        missile.type = ServerEntityType::EnemyMissile;
        missile.posX = boss.posX - 55.0f;
        missile.posY = boss.posY + 35.0f;
        const float angle = angleDegrees * 0.017453292519943295f;
        const float speed = std::min(
        440.0f + 30.0f * static_cast<float>(_stageNumber - 1), 600.0f);
        missile.velX = -speed * std::cos(angle);
        missile.velY = speed * std::sin(angle);
        _entities[missile.id] = missile;
        broadcastSpawnEntity(missile);
      }
    }

    void checkCollisions(std::vector<network::NetworkId>& destroyed) {
      std::vector<std::pair<float, float>> bonusDrops;
      for (auto& [entityId, entity] : _entities) {
        if (isMarkedDestroyed(destroyed, entityId)) continue;
        if (isPlayerMissile(entity.type)) {
          checkPlayerMissileCollision(entity, destroyed, bonusDrops);
        } else if (isEnemyMissile(entity.type)) {
          checkEnemyMissileCollision(entity, destroyed);
        } else if (isBonus(entity.type)) {
          checkBonusPickup(entity, destroyed);
        } else if (entity.type == ServerEntityType::Bydos ||
                   entity.type == ServerEntityType::Boss) {
          checkRammingCollision(entity, destroyed, bonusDrops);
        }
      }
      for (const auto& [x, y] : bonusDrops) spawnBonus(x, y);
    }

    void checkPlayerMissileCollision(
      const ServerEntity& missile, std::vector<network::NetworkId>& destroyed,
      std::vector<std::pair<float, float>>& bonusDrops) {
      for (auto& [targetId, target] : _entities) {
        if (targetId == missile.id || isMarkedDestroyed(destroyed, targetId)) continue;
        if (target.type != ServerEntityType::Bydos &&
            target.type != ServerEntityType::Boss) continue;
        if (target.type == ServerEntityType::Boss) {
          if (!missileHitsBoss(missile, target)) continue;
        } else if (std::abs(missile.posX - target.posX) >= 33.0f ||
                   std::abs(missile.posY - target.posY) >= 33.0f) {
          continue;
        }
        destroyed.push_back(missile.id);
        target.hp -= missile.damage;
        broadcastHealthUpdate(targetId, target.hp, target.maxHp);
        if (target.hp > 0) break;
        destroyed.push_back(targetId);
        awardScore(missile.ownerId,
                   target.type == ServerEntityType::Boss ? 1000u : 100u);
        if (target.type == ServerEntityType::Bydos) {
          ++_killsThisStage;
          bonusDrops.emplace_back(target.posX, target.posY);
        }
        break;
      }
    }

    void checkEnemyMissileCollision(const ServerEntity& missile,
                                    std::vector<network::NetworkId>& destroyed) {
      for (auto& [id, client] : _clients) {
        if (client.dead || std::abs(missile.posX - client.posX) >= 33.0f ||
            std::abs(missile.posY - client.posY) >= 17.0f) continue;
        destroyed.push_back(missile.id);
        damagePlayer(client, 1);
        break;
      }
    }

    void checkBonusPickup(const ServerEntity& bonus,
                          std::vector<network::NetworkId>& destroyed) {
      for (auto& [id, client] : _clients) {
        if (client.dead || std::abs(bonus.posX - client.posX) >= 28.0f ||
            std::abs(bonus.posY - client.posY) >= 28.0f) continue;
        destroyed.push_back(bonus.id);
        applyBonus(client, bonus.type);
        break;
      }
    }

    void checkRammingCollision(
      const ServerEntity& enemy, std::vector<network::NetworkId>& destroyed,
      std::vector<std::pair<float, float>>& bonusDrops) {
      for (auto& [id, client] : _clients) {
        if (client.dead) continue;
        const bool boss = enemy.type == ServerEntityType::Boss;
        if (boss) {
          if (!rectanglesOverlap(client.posX, client.posY, PLAYER_WIDTH, PLAYER_HEIGHT,
                                 enemy.posX, enemy.posY, BOSS_WIDTH, BOSS_HEIGHT)) continue;
        } else if (std::abs(enemy.posX - client.posX) >= 33.0f ||
                   std::abs(enemy.posY - client.posY) >= 25.0f) {
          continue;
        }
        if (!boss) {
          destroyed.push_back(enemy.id);
          ++_killsThisStage;
          bonusDrops.emplace_back(enemy.posX, enemy.posY);
        }
        damagePlayer(client, 1);
        break;
      }
    }

    void damagePlayer(ConnectedClient& client, int amount) {
      if (client.dead || client.hitCooldown > 0.0f) return;
      client.hitCooldown = 0.65f;
      client.hp = std::max(0, client.hp - amount);
      broadcastHealthUpdate(client.id, client.hp, client.maxHp);
      if (client.hp != 0) return;
      client.dead = true;
      client.deathTimer = 1.5f;
      client.velX = 0.0f;
      client.velY = 0.0f;
      client.shootPressed = false;
      clearAugments(client);
    }

    void applyBonus(ConnectedClient& client, ServerEntityType type) {
      switch (type) {
        case ServerEntityType::BonusDouble:
          if (client.shotPattern == 2) {
            refreshShotPattern(client, 2);
          } else {
            refreshShotPattern(client, 1);
          }
          break;
        case ServerEntityType::BonusTriple:
          refreshShotPattern(client, 2);
          break;
        case ServerEntityType::BonusHeal: {
          const int oldHealth = client.hp;
          client.hp = std::min(client.maxHp, client.hp + 2);
          if (client.hp != oldHealth)
            broadcastHealthUpdate(client.id, client.hp, client.maxHp);
          break;
        }
        case ServerEntityType::BonusDamage:
          client.damageLevel = static_cast<std::uint8_t>(
            std::min(2, static_cast<int>(client.damageLevel) + 1));
          client.damageRemaining = AUGMENT_DURATION;
          sendAugmentUpdate(client.id, 3, client.damageLevel,
                            client.damageRemaining);
          break;
        default:
          break;
      }
    }

    void refreshShotPattern(ConnectedClient& client, std::uint8_t pattern) {
      if (client.shotPattern != 0 && client.shotPattern != pattern)
        sendAugmentUpdate(client.id, client.shotPattern, 0, 0.0f);
      client.shotPattern = pattern;
      client.shotPatternRemaining = AUGMENT_DURATION;
      sendAugmentUpdate(client.id, pattern, 1, client.shotPatternRemaining);
    }

    void clearAugments(ConnectedClient& client) {
      if (client.shotPattern != 0)
        sendAugmentUpdate(client.id, client.shotPattern, 0, 0.0f);
      if (client.damageLevel > 0)
        sendAugmentUpdate(client.id, 3, 0, 0.0f);
      client.shotPattern = 0;
      client.shotPatternRemaining = 0.0f;
      client.damageLevel = 0;
      client.damageRemaining = 0.0f;
    }

    void sendActiveAugments(const ConnectedClient& client,
                            const network::Endpoint& recipient) {
      if (client.shotPattern != 0)
        sendAugmentUpdate(client.id, client.shotPattern, 1,
                          client.shotPatternRemaining, recipient);
      if (client.damageLevel > 0)
        sendAugmentUpdate(client.id, 3, client.damageLevel,
                          client.damageRemaining, recipient);
    }

    void awardScore(network::NetworkId playerId, std::uint32_t points) {
      auto it = _clients.find(playerId);
      if (it == _clients.end() || points == 0) return;
      const auto maximum = std::numeric_limits<std::uint32_t>::max();
      it->second.score = it->second.score > maximum - points
        ? maximum : it->second.score + points;
      broadcastScoreUpdate(it->second);
    }

    void spawnBonus(float x, float y) {
      if (!canSpawnEntity()) return;
      const int chance = _currentDifficulty == 0 ? 40 :
        (_currentDifficulty == 2 ? 20 : 30);
      if (randomInt(0, 99) >= chance) return;
      ServerEntity bonus;
      bonus.id = allocateEntityId();
      if (bonus.id == 0) return;
      bonus.type = static_cast<ServerEntityType>(randomInt(4, 7));
      bonus.posX = x;
      bonus.posY = y;
      bonus.velX = -80.0f;
      bonus.hp = 1;
      bonus.maxHp = 1;
      _entities[bonus.id] = bonus;
      broadcastSpawnEntity(bonus);
    }

    template<typename T>
    static void sortUnique(std::vector<T>& values) {
      std::sort(values.begin(), values.end());
      values.erase(std::unique(values.begin(), values.end()), values.end());
    }

    static bool isMarkedDestroyed(const std::vector<network::NetworkId>& ids,
                                  network::NetworkId id) {
      return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    static bool isPlayerMissile(ServerEntityType type) {
      return type == ServerEntityType::PlayerMissile ||
             type == ServerEntityType::PlayerMissileHeavy;
    }

    static bool isEnemyMissile(ServerEntityType type) {
      return type == ServerEntityType::EnemyMissile;
    }

    static bool isBonus(ServerEntityType type) {
      return type == ServerEntityType::BonusDouble ||
             type == ServerEntityType::BonusTriple ||
             type == ServerEntityType::BonusHeal ||
             type == ServerEntityType::BonusDamage;
    }

    int randomInt(int minimum, int maximum) {
      return std::uniform_int_distribution<int>(minimum, maximum)(_rng);
    }

    void broadcastSpawnEntity(const ServerEntity& ent) {
      for (const auto& [id, client] : _clients)
        sendSpawnEntity(ent, client.endpoint);
    }

    void broadcastHealthUpdate(network::NetworkId netId, int hp, int maxHp) {
      const auto health = static_cast<std::uint8_t>(std::clamp(hp, 0, 255));
      const auto maximum = static_cast<std::uint8_t>(std::clamp(maxHp, 0, 255));
      for (const auto& [id, client] : _clients)
        sendHealthUpdate(netId, health, maximum, client.endpoint);
    }

    void sendAugmentUpdate(network::NetworkId netId, std::uint8_t augment,
                           std::uint8_t level, float remaining) {
      for (const auto& [id, client] : _clients) {
        sendAugmentUpdate(netId, augment, level, remaining, client.endpoint);
      }
    }

    void sendAugmentUpdate(network::NetworkId netId, std::uint8_t augment,
                           std::uint8_t level, float remaining,
                           const network::Endpoint& recipient) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::AugmentUpdate,
        _outgoingSeq++, 10
      };
      network::PacketStreamWriter writer;
      writer << header << netId << augment << level << remaining;
      sendDatagram(writer.getBuffer(), recipient);
    }

    void sendScoreUpdate(const ConnectedClient& client,
                         const network::Endpoint& recipient) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::ScoreUpdate,
        _outgoingSeq++, 8
      };
      network::PacketStreamWriter writer;
      writer << header << client.id << client.score;
      sendDatagram(writer.getBuffer(), recipient);
    }

    void broadcastScoreUpdate(const ConnectedClient& client) {
      for (const auto& [id, recipient] : _clients)
        sendScoreUpdate(client, recipient.endpoint);
    }

    void broadcastDestroyEntity(network::NetworkId id) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::DestroyEntity,
        _outgoingSeq++, sizeof(id)
      };
      network::PacketStreamWriter writer;
      writer << header << id;
      for (const auto& [clientId, client] : _clients)
        sendDatagram(writer.getBuffer(), client.endpoint);
    }

    void destroyEntity(network::NetworkId entId) {
      const auto it = _entities.find(entId);
      if (it == _entities.end()) return;
      const ServerEntityType type = it->second.type;
      _entities.erase(it);
      broadcastDestroyEntity(entId);
      if (type == ServerEntityType::Boss) onBossDefeated();
    }

    void broadcastGameState() {
      if (_clients.empty()) return;
      sendInputAcks();

      network::SnapshotState current;
      current.reserve(_clients.size() + _entities.size());
      for (const auto& [id, client] : _clients) {
        const network::EntityStateData state{
          client.id, client.posX, client.posY, client.velX, client.velY
        };
        current.emplace(id, network::packEntityState(state));
      }
      for (const auto& [id, entity] : _entities) {
        const network::EntityStateData state{
          entity.id, entity.posX, entity.posY, entity.velX, entity.velY
        };
        current.emplace(id, network::packEntityState(state));
      }

      ++_snapshotTick;
      const bool keyframe = _forceFullSnapshot || !_hasPreviousSnapshot ||
        (_snapshotTick % SNAPSHOT_KEYFRAME_INTERVAL == 0);
      const bool someoneNeedsFull = std::any_of(_clients.begin(), _clients.end(),
        [](const auto& entry) { return entry.second.needsFullSnapshot; });
      network::PacketData deltaPacket;
      if (!keyframe) {
        const network::SnapshotDelta delta =
          network::makeSnapshotDelta(_previousSnapshot, _previousSnapshotTick, current, _simulationTick);
        if (delta.changed.empty() && delta.removed.empty() && !someoneNeedsFull) return;
        network::PacketStreamWriter payload;
        network::writeSnapshotDelta(payload, delta);
        deltaPacket = snapshotPacket(network::PacketType::GameStateSyncDelta, payload.getBuffer());
      }
      network::PacketData fullPacket;
      if (keyframe || someoneNeedsFull)
        fullPacket = snapshotPacket(network::PacketType::GameStateSyncPacked, fullSnapshotPayload(current));
      for (auto& [id, client] : _clients) {
        const bool full = keyframe || client.needsFullSnapshot;
        const network::PacketData& packet = full ? fullPacket : deltaPacket;
        sendDatagram(packet, client.endpoint, false);
        if (_metrics) _metrics->recordSnapshot(full, packet.size());
        client.needsFullSnapshot = false;
      }

      _previousSnapshot = std::move(current);
      _previousSnapshotTick = _simulationTick;
      _hasPreviousSnapshot = true;
      _forceFullSnapshot = false;
    }

    network::PacketData fullSnapshotPayload(const network::SnapshotState& state) const {
      network::PacketStreamWriter payload;
      payload << _simulationTick << static_cast<std::uint16_t>(state.size());
      for (const auto& [id, packed] : state) payload << packed;
      return payload.getBuffer();
    }

    network::PacketData snapshotPacket(network::PacketType type, const network::PacketData& payload) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++, static_cast<std::uint16_t>(payload.size())
      };
      network::PacketStreamWriter writer;
      writer << header;
      auto packet = writer.getBuffer();
      packet.insert(packet.end(), payload.begin(), payload.end());
      return packet;
    }

    void sendDatagram(const network::PacketData& packet,
                      const network::Endpoint& recipient,
                      bool reliable = true) {
      if (reliable && _reliableQueue) {
        _reliableQueue->send(_socket, packet, recipient);
        return;
      }
      _socket.send(packet, recipient);
      if (_metrics) _metrics->recordSend(packet.size());
    }

    static bool isNewerSequence(std::uint32_t candidate,
                                std::uint32_t previous, bool initialized) {
      if (!initialized) return true;
      const std::uint32_t delta = candidate - previous;
      return delta != 0 && delta < 0x80000000u;
    }

    std::uint64_t generateSessionToken() {
      std::random_device source;
      std::uint64_t token = static_cast<std::uint64_t>(source()) << 32;
      token |= static_cast<std::uint64_t>(source());
      return token == 0 ? 1 : token;
    }

    network::INetworkSocket& _socket;
    RoomId _roomId{1};
    std::size_t _maxPlayers{4};
    bool _fixedDifficulty{false};
    std::unordered_map<network::NetworkId, ConnectedClient> _clients;
    std::unordered_map<network::NetworkId, ServerEntity> _entities;
    network::NetworkId _nextPlayerId{1000};
    network::NetworkId _nextEntityId{2000};
    std::uint32_t& _outgoingSeq;
    ReliableUdpQueue* _reliableQueue{nullptr};
    ServerMetrics* _metrics{nullptr};
    std::uint8_t _currentDifficulty{1};
    float _tickAccumulator{0.0f};
    double _droppedSimulationSeconds{0.0};
    std::uint64_t _snapshotTick{0};
    std::uint32_t _simulationTick{0};
    bool _forceFullSnapshot{true};
    network::SnapshotState _previousSnapshot;
    std::uint32_t _previousSnapshotTick{0};
    bool _hasPreviousSnapshot{false};
    float _bydoSpawnTimer{0.f};
    float _enemyShootTimer{0.f};
    float _bossShootTimer{0.f};
    std::uint32_t _stageNumber{1};
    std::uint32_t _killsThisStage{0};
    bool _bossActive{false};
    std::mt19937 _rng;
    static constexpr std::size_t MAX_ACTIVE_BYDOS = 12;
  };

}

#endif // !GAMEROOM_HPP
