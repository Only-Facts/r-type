#include "GameRoom.hpp"
#include "network/SnapshotDelta.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

using namespace rtype;

class GameplayMockSocket final : public network::INetworkSocket {
public:
  struct SentPacket {
    network::PacketData data;
    network::Endpoint endpoint;
  };

  void bind(unsigned short = 0) override {}
  void setNonBlocking(bool) override {}
  unsigned short getLocalPort() const override { return 4242; }
  void send(const network::PacketData& data,
            const network::Endpoint& endpoint) override {
    sent.push_back({data, endpoint});
  }
  bool receive(network::PacketData& data,
               network::Endpoint& endpoint) override {
    std::lock_guard<std::mutex> lock(mutex);
    if (incoming.empty()) return false;
    data = std::move(incoming.front().data);
    endpoint = std::move(incoming.front().endpoint);
    incoming.pop_front();
    return true;
  }

  std::mutex mutex;
  std::deque<SentPacket> incoming;
  std::vector<SentPacket> sent;
};

namespace rtype::server {
struct GameRoomTestAccess {
  static ConnectedClient& addClient(GameRoom& room,
                                    network::NetworkId id = 1000) {
    ConnectedClient client;
    client.id = id;
    client.endpoint = {"127.0.0.1", static_cast<unsigned short>(5000 + id % 1000)};
    client.posX = 200.0f;
    client.posY = 300.0f;
    client.hp = 3;
    client.maxHp = 5;
    client.sessionToken = 0x1122334455667788ull;
    return room._clients.emplace(id, client).first->second;
  }

  static void addEntity(GameRoom& room, ServerEntity entity) {
    room._entities[entity.id] = entity;
  }

  static void update(GameRoom& room, float dt) { room.updateGameLogic(dt); }

  static void removeClient(GameRoom& room, network::NetworkId id) {
    room.removeClient(id);
  }

  static std::size_t ownedMissiles(const GameRoom& room,
                                  network::NetworkId ownerId) {
    return static_cast<std::size_t>(std::count_if(
      room._entities.begin(), room._entities.end(),
      [ownerId](const auto& entry) {
        return GameRoom::isPlayerMissile(entry.second.type) &&
               entry.second.ownerId == ownerId;
      }));
  }

  static void setKills(GameRoom& room, std::uint32_t kills) {
    room._killsThisStage = kills;
  }

  static void removeEntity(GameRoom& room, network::NetworkId id) { room._entities.erase(id); }

  static bool hasEntity(const GameRoom& room, network::NetworkId id) {
    return room._entities.find(id) != room._entities.end();
  }

  static network::NetworkId bossId(const GameRoom& room) {
    for (const auto& [id, entity] : room._entities) {
      if (entity.type == ServerEntityType::Boss) return id;
    }
    return 0;
  }

  static void setEntityHealth(GameRoom& room, network::NetworkId id, int hp) {
    room._entities.at(id).hp = hp;
  }

  static void damagePlayer(GameRoom& room, network::NetworkId id, int damage) {
    room.damagePlayer(room._clients.at(id), damage);
  }

  static void advanceAugments(GameRoom& room, network::NetworkId id, float dt) {
    room.updateAugmentTimers(room._clients.at(id), dt);
  }

  static void broadcastSnapshot(GameRoom& room) {
    room.broadcastGameState();
  }

  static void clearEntities(GameRoom& room, ServerEntityType type) {
    for (auto it = room._entities.begin(); it != room._entities.end();) {
      if (it->second.type == type) it = room._entities.erase(it);
      else ++it;
    }
  }
};
}

static network::PacketType packetType(const GameplayMockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  reader >> header;
  return header.type;
}

static std::uint8_t spawnedEntityType(
  const GameplayMockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  network::NetworkId id{};
  std::uint8_t type{};
  float x{}, y{};
  reader >> header >> id >> type >> x >> y;
  return type;
}

static std::uint32_t scoreValue(
  const GameplayMockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  network::NetworkId id{};
  std::uint32_t score{};
  reader >> header >> id >> score;
  return score;
}

static std::vector<GameplayMockSocket::SentPacket> packetsOf(
  const GameplayMockSocket& socket, std::uint8_t type) {
  std::vector<GameplayMockSocket::SentPacket> result;
  for (const auto& packet : socket.sent) {
    if (static_cast<std::uint8_t>(packetType(packet)) == type)
      result.push_back(packet);
  }
  return result;
}


static void testSharedPlayerMovement() {
  const gameplay::MovementBounds bounds{1920.0f, 1080.0f, 33.0f, 34.0f};
  auto position = gameplay::applyMovement({100.0f, 100.0f}, {false, false, false, true}, 1.0f / 60.0f, bounds);
  assert(std::abs(position.x - 105.0f) < 0.001f && position.y == 100.0f);

  const auto diagonal = gameplay::movementVelocity({true, false, false, true});
  assert(std::abs(std::hypot(diagonal.x, diagonal.y) - gameplay::PLAYER_SPEED) < 0.01f);
  assert(diagonal.x > 0.0f && diagonal.y < 0.0f);

  position = gameplay::applyMovement({1880.0f, 1040.0f}, {false, true, false, true}, 1.0f, bounds);
  assert(position.x == 1920.0f - 33.0f && position.y == 1080.0f - 34.0f);
  position = gameplay::applyMovement({2.0f, 2.0f}, {true, false, true, false}, 1.0f, bounds);
  assert(position.x == 0.0f && position.y == 0.0f);
}

static void testBonusPickupsAndAugments() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 200.0f;
  player.posY = 300.0f;

  server::ServerEntity heal;
  heal.id = 20;
  heal.type = server::ServerEntityType::BonusHeal;
  heal.posX = player.posX;
  heal.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, heal);
  server::GameRoomTestAccess::update(room, 0.016f);
  assert(player.hp == 5);
  assert(room.entityCount(server::ServerEntityType::BonusHeal) == 0);
  assert(!packetsOf(socket, 0x13).empty());

  server::ServerEntity doubleShot;
  doubleShot.id = 21;
  doubleShot.type = server::ServerEntityType::BonusDouble;
  doubleShot.posX = player.posX;
  doubleShot.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, doubleShot);
  server::GameRoomTestAccess::update(room, 0.016f);
  assert(room.augmentLevel(player.id, 1) == 1);

  player.shootPressed = true;
  player.shootCooldown = 0.0f;
  server::GameRoomTestAccess::update(room, 0.0f);
  assert(room.entityCount(server::ServerEntityType::PlayerMissile) == 2);
  server::GameRoomTestAccess::clearEntities(
    room, server::ServerEntityType::PlayerMissile);
  player.shootPressed = false;

  server::ServerEntity triple;
  triple.id = 22;
  triple.type = server::ServerEntityType::BonusTriple;
  triple.posX = player.posX;
  triple.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, triple);
  server::GameRoomTestAccess::update(room, 0.016f);
  assert(room.augmentLevel(player.id, 2) == 1);
  assert(room.augmentLevel(player.id, 1) == 0);

  server::ServerEntity refreshDouble;
  refreshDouble.id = 23;
  refreshDouble.type = server::ServerEntityType::BonusDouble;
  refreshDouble.posX = player.posX;
  refreshDouble.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, refreshDouble);
  server::GameRoomTestAccess::update(room, 0.016f);
  assert(room.augmentLevel(player.id, 2) == 1);

  for (network::NetworkId id : {24u, 25u, 26u}) {
    server::ServerEntity damage;
    damage.id = id;
    damage.type = server::ServerEntityType::BonusDamage;
    damage.posX = player.posX;
    damage.posY = player.posY;
    server::GameRoomTestAccess::addEntity(room, damage);
    server::GameRoomTestAccess::update(room, 0.016f);
  }
  assert(room.augmentLevel(player.id, 3) == 2);

  player.shootPressed = true;
  player.shootCooldown = 0.0f;
  server::GameRoomTestAccess::update(room, 0.0f);
  assert(room.entityCount(server::ServerEntityType::PlayerMissileHeavy) == 3);

  server::GameRoomTestAccess::advanceAugments(room, player.id, 10.1f);
  assert(room.augmentLevel(player.id, 2) == 0);
  assert(room.augmentLevel(player.id, 3) == 0);
  server::GameRoomTestAccess::clearEntities(
    room, server::ServerEntityType::PlayerMissileHeavy);
  server::ServerEntity tripleAgain;
  tripleAgain.id = 27;
  tripleAgain.type = server::ServerEntityType::BonusTriple;
  tripleAgain.posX = player.posX;
  tripleAgain.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, tripleAgain);
  server::ServerEntity damageAgain;
  damageAgain.id = 28;
  damageAgain.type = server::ServerEntityType::BonusDamage;
  damageAgain.posX = player.posX;
  damageAgain.posY = player.posY;
  server::GameRoomTestAccess::addEntity(room, damageAgain);
  server::GameRoomTestAccess::update(room, 0.0f);
  server::GameRoomTestAccess::damagePlayer(room, player.id, 99);
  assert(room.augmentLevel(player.id, 2) == 0);
  assert(room.augmentLevel(player.id, 3) == 0);
  const auto augmentPackets = packetsOf(socket, 0x14);
  assert(augmentPackets.size() >= 5);
}

static void testBossHitboxMatchesItsSprite() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 100.0f;
  player.posY = 100.0f;

  server::ServerEntity boss;
  boss.id = 40;
  boss.type = server::ServerEntityType::Boss;
  boss.posX = 1600.0f;
  boss.posY = 400.0f;
  boss.baseY = 400.0f;
  boss.hp = 50;
  boss.maxHp = 50;
  server::GameRoomTestAccess::addEntity(room, boss);

  server::ServerEntity lowerHalf;
  lowerHalf.id = 41;
  lowerHalf.type = server::ServerEntityType::PlayerMissile;
  lowerHalf.posX = 1650.0f;
  lowerHalf.posY = 600.0f;
  lowerHalf.ownerId = player.id;
  server::GameRoomTestAccess::addEntity(room, lowerHalf);

  server::ServerEntity aboveSprite;
  aboveSprite.id = 42;
  aboveSprite.type = server::ServerEntityType::PlayerMissile;
  aboveSprite.posX = 1520.0f;
  aboveSprite.posY = 350.0f;
  aboveSprite.ownerId = player.id;
  server::GameRoomTestAccess::addEntity(room, aboveSprite);

  server::GameRoomTestAccess::update(room, 0.001f);
  assert(!server::GameRoomTestAccess::hasEntity(room, lowerHalf.id));
  assert(server::GameRoomTestAccess::hasEntity(room, aboveSprite.id));
  assert(!packetsOf(socket, 0x13).empty());
}

static void testBossProgressionAndScore() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 100.0f;
  player.posY = 100.0f;
  server::GameRoomTestAccess::setKills(room, 9);

  server::ServerEntity enemy;
  enemy.id = 30;
  enemy.type = server::ServerEntityType::Bydos;
  enemy.posX = 500.0f;
  enemy.posY = 500.0f;
  enemy.hp = 1;
  enemy.maxHp = 1;
  server::GameRoomTestAccess::addEntity(room, enemy);

  server::ServerEntity shot;
  shot.id = 31;
  shot.type = server::ServerEntityType::PlayerMissile;
  shot.posX = enemy.posX;
  shot.posY = enemy.posY;
  shot.ownerId = player.id;
  server::GameRoomTestAccess::addEntity(room, shot);

  server::GameRoomTestAccess::update(room, 0.001f);
  assert(room.bossActive());
  assert(room.stageNumber() == 1);
  assert(room.entityCount(server::ServerEntityType::Boss) == 1);
  assert(room.scoreFor(player.id) == 100);
  const auto spawnPackets = packetsOf(socket, 0x12);
  assert(std::any_of(spawnPackets.begin(), spawnPackets.end(),
    [](const auto& packet) { return spawnedEntityType(packet) == 9; }));

  const auto bossId = server::GameRoomTestAccess::bossId(room);
  assert(bossId != 0);
  server::GameRoomTestAccess::setEntityHealth(room, bossId, 1);
  server::ServerEntity finisher;
  finisher.id = 32;
  finisher.type = server::ServerEntityType::PlayerMissileHeavy;
  finisher.posX = 1810.0f;
  finisher.posY = 540.0f;
  finisher.ownerId = player.id;
  finisher.damage = 1;
  server::GameRoomTestAccess::addEntity(room, finisher);
  server::GameRoomTestAccess::update(room, 0.001f);

  assert(!room.bossActive());
  assert(room.stageNumber() == 2);
  assert(room.killsThisStage() == 0);
  assert(room.entityCount(server::ServerEntityType::Boss) == 0);
  assert(room.scoreFor(player.id) == 1100);
  const auto scores = packetsOf(socket, 0x15);
  assert(!scores.empty());
  assert(std::any_of(scores.begin(), scores.end(),
    [](const auto& packet) { return scoreValue(packet) == 1100; }));
}

static network::PacketData playerInputPacket(
    std::uint32_t sequence, network::NetworkId playerId,
    std::uint64_t token, std::uint8_t up, std::uint8_t down,
    std::uint8_t left, std::uint8_t right, std::uint8_t shoot) {
  network::PacketStreamWriter payload;
  payload << playerId << token << up << down << left << right << shoot;
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, network::PacketType::PlayerInput, sequence,
    static_cast<std::uint16_t>(payload.getBuffer().size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto packet = writer.getBuffer();
  packet.insert(packet.end(), payload.getBuffer().begin(),
                payload.getBuffer().end());
  return packet;
}

struct InputAckInfo {
  std::uint32_t lastInput{0};
  std::uint32_t tick{0};
  float x{0.0f};
  float y{0.0f};
};

static std::optional<InputAckInfo> lastInputAck(const GameplayMockSocket& socket) {
  std::optional<InputAckInfo> found;
  for (const auto& sent : socket.sent) {
    network::PacketStreamReader reader(sent.data);
    network::PacketHeader header{};
    reader >> header;
    if (header.type != network::PacketType::InputAck) continue;
    InputAckInfo info;
    reader >> info.lastInput >> info.tick >> info.x >> info.y;
    found = info;
  }
  return found;
}

static void testInputsAreAppliedOneStepEachAndAcknowledged() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 100.0f;
  player.posY = 100.0f;
  const float step = gameplay::PLAYER_SPEED * gameplay::INPUT_STEP_SECONDS;

  for (std::uint32_t input = 10; input <= 12; ++input)
    room.handlePacket(playerInputPacket(input, player.id, player.sessionToken, 0, 0, 0, 1, 0), player.endpoint);

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  assert(std::abs(player.posX - (100.0f + 2.0f * step)) < 0.001f);
  assert(player.lastProcessedInput == 11);

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  assert(std::abs(player.posX - (100.0f + 3.0f * step)) < 0.001f);
  assert(player.lastProcessedInput == 12);

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  assert(std::abs(player.posX - (100.0f + 3.0f * step)) < 0.001f);
  assert(player.velX == gameplay::PLAYER_SPEED);

  server::GameRoomTestAccess::broadcastSnapshot(room);
  const auto ack = lastInputAck(socket);
  assert(ack.has_value());
  assert(ack->lastInput == 12);
  assert(ack->tick == 3);
  assert(std::abs(ack->x - player.posX) < 0.001f && ack->y == 100.0f);
}

static void testInputQueueIsBounded() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 100.0f;
  player.posY = 100.0f;
  const float step = gameplay::PLAYER_SPEED * gameplay::INPUT_STEP_SECONDS;

  for (std::uint32_t input = 1; input <= 20; ++input)
    room.handlePacket(playerInputPacket(input, player.id, player.sessionToken, 0, 0, 0, 1, 0), player.endpoint);
  assert(player.pendingInputs.size() == 8);

  for (int tick = 0; tick < 10; ++tick) server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  assert(player.lastProcessedInput == 20);
  assert(std::abs(player.posX - (100.0f + 8.0f * step)) < 0.001f);
}

static void testDeadPlayerInputsAreAcknowledgedWithoutMoving() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 300.0f;
  player.posY = 300.0f;
  player.dead = true;
  player.deathTimer = 10.0f;

  room.handlePacket(playerInputPacket(5, player.id, player.sessionToken, 1, 0, 0, 1, 1), player.endpoint);
  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  assert(player.lastProcessedInput == 5);
  assert(player.posX == 300.0f && player.posY == 300.0f);
  assert(!player.shootPressed);
}

static void testSessionValidationAndInputSequenceWrap() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  const auto endpoint = player.endpoint;
  const auto startX = player.posX;
  const auto token = player.sessionToken;

  room.handlePacket(playerInputPacket(100, player.id, token + 1,
    0, 0, 0, 1, 0), endpoint);
  server::GameRoomTestAccess::update(room, 0.1f);
  assert(player.posX == startX);

  room.handlePacket(playerInputPacket(0xFFFFFFFEu, player.id, token,
    0, 0, 0, 1, 0), endpoint);
  server::GameRoomTestAccess::update(room, 0.1f);
  assert(player.posX > startX);
  const auto afterFirst = player.posX;

  room.handlePacket(playerInputPacket(1u, player.id, token,
    0, 0, 1, 0, 0), endpoint);
  server::GameRoomTestAccess::update(room, 0.1f);
  assert(player.posX < afterFirst);
  const auto afterWrap = player.posX;

  room.handlePacket(playerInputPacket(0xFFFFFFFFu, player.id, token,
    0, 0, 0, 1, 0), endpoint);
  assert(player.velX == -300.0f);

  room.handlePacket(playerInputPacket(2u, player.id, token,
    0, 0, 0, 1, 0), {"127.0.0.2", endpoint.port});
  assert(player.velX == -300.0f);
  assert(player.posX <= afterWrap);
}

static void testRammingDoesNotAwardScore() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 300.0f;
  player.posY = 300.0f;
  server::ServerEntity enemy;
  enemy.id = 40;
  enemy.type = server::ServerEntityType::Bydos;
  enemy.posX = player.posX;
  enemy.posY = player.posY;
  enemy.hp = 1;
  enemy.maxHp = 1;
  server::GameRoomTestAccess::addEntity(room, enemy);

  server::GameRoomTestAccess::update(room, 0.001f);
  assert(room.scoreFor(player.id) == 0);
  assert(room.entityCount(server::ServerEntityType::Bydos) == 0);
  assert(player.hp == 2);
}


static std::pair<network::PacketType, std::uint16_t> snapshotInfo(
    const GameplayMockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  reader >> header;
  if (header.type == network::PacketType::GameStateSyncDelta) {
    const auto delta = network::readSnapshotDelta(reader, 256);
    return {header.type, static_cast<std::uint16_t>(delta.changed.size())};
  }
  std::uint32_t tick{};
  std::uint16_t count{};
  reader >> tick >> count;
  assert(header.payloadSize == reader.remaining() + sizeof(tick) + sizeof(count));
  return {header.type, count};
}

static std::uint32_t snapshotTick(const GameplayMockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  std::uint32_t tick{};
  reader >> header >> tick;
  return tick;
}

static network::SnapshotDelta lastDelta(const GameplayMockSocket& socket) {
  network::PacketStreamReader reader(socket.sent.back().data);
  network::PacketHeader header{};
  reader >> header;
  assert(header.type == network::PacketType::GameStateSyncDelta);
  return network::readSnapshotDelta(reader, 256);
}

static void testSnapshotDeltaNamesItsBaseAndLifecycle() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);
  player.posX = 100.0f;
  player.posY = 100.0f;

  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(packetType(socket.sent.back()) == network::PacketType::GameStateSyncPacked);
  const std::uint32_t firstTick = snapshotTick(socket.sent.back());

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  server::ServerEntity enemy;
  enemy.id = 2500;
  enemy.type = server::ServerEntityType::Bydos;
  enemy.posX = 1500.0f;
  enemy.posY = 500.0f;
  server::GameRoomTestAccess::addEntity(room, enemy);
  server::GameRoomTestAccess::broadcastSnapshot(room);
  auto delta = lastDelta(socket);
  assert(delta.baseTick == firstTick);
  assert(delta.tick == firstTick + 1);
  assert(delta.removed.empty());
  assert(std::any_of(delta.changed.begin(), delta.changed.end(), [](const auto& record) {
    return record.state.netId == 2500 && record.mask == network::DELTA_ALL_FIELDS;
  }));

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  server::GameRoomTestAccess::removeEntity(room, 2500);
  server::GameRoomTestAccess::broadcastSnapshot(room);
  delta = lastDelta(socket);
  assert(delta.baseTick == firstTick + 1);
  assert(delta.removed.size() == 1 && delta.removed.front() == 2500);
}

static network::PacketData snapshotRequestPacket(network::NetworkId id, std::uint64_t token) {
  network::PacketStreamWriter payload;
  payload << id << token;
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, network::PacketType::SnapshotRequest, 7,
    static_cast<std::uint16_t>(payload.getBuffer().size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto packet = writer.getBuffer();
  packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());
  return packet;
}

static network::PacketType lastTypeFor(const GameplayMockSocket& socket, unsigned short port) {
  for (auto sent = socket.sent.rbegin(); sent != socket.sent.rend(); ++sent)
    if (sent->endpoint.port == port) return packetType(*sent);
  return network::PacketType::Ping;
}

static void testFullSnapshotIsSentOnlyToTheRequester() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& first = server::GameRoomTestAccess::addClient(room, 1000);
  auto& second = server::GameRoomTestAccess::addClient(room, 1001);
  first.posX = 100.0f;
  second.posX = 300.0f;

  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(lastTypeFor(socket, first.endpoint.port) == network::PacketType::GameStateSyncPacked);
  assert(lastTypeFor(socket, second.endpoint.port) == network::PacketType::GameStateSyncPacked);

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  first.posX += 8.0f;
  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(lastTypeFor(socket, first.endpoint.port) == network::PacketType::GameStateSyncDelta);
  assert(lastTypeFor(socket, second.endpoint.port) == network::PacketType::GameStateSyncDelta);

  room.handlePacket(snapshotRequestPacket(second.id, second.sessionToken + 1), second.endpoint);
  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  const std::size_t before = socket.sent.size();
  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(socket.sent.size() == before);

  room.handlePacket(snapshotRequestPacket(second.id, second.sessionToken), second.endpoint);
  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(lastTypeFor(socket, first.endpoint.port) == network::PacketType::GameStateSyncDelta);
  assert(lastTypeFor(socket, second.endpoint.port) == network::PacketType::GameStateSyncPacked);

  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  first.posX += 8.0f;
  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(lastTypeFor(socket, second.endpoint.port) == network::PacketType::GameStateSyncDelta);
}

static void testSnapshotDeltaAndKeyframeCadence() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& player = server::GameRoomTestAccess::addClient(room);

  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(socket.sent.size() == 1);
  auto [type, count] = snapshotInfo(socket.sent.back());
  assert(type == network::PacketType::GameStateSyncPacked);
  assert(count == 1);
  assert(snapshotTick(socket.sent.back()) == 0);

  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(socket.sent.size() == 1);
  server::GameRoomTestAccess::update(room, 1.0f / 60.0f);
  player.posX += 8.0f;
  server::GameRoomTestAccess::broadcastSnapshot(room);
  assert(socket.sent.size() == 2);
  std::tie(type, count) = snapshotInfo(socket.sent.back());
  assert(type == network::PacketType::GameStateSyncDelta);
  assert(count == 1);
  assert(snapshotTick(socket.sent.back()) == 1);

  for (int tick = 3; tick < 60; ++tick)
    server::GameRoomTestAccess::broadcastSnapshot(room);
  std::tie(type, count) = snapshotInfo(socket.sent.back());
  assert(type == network::PacketType::GameStateSyncPacked);
  assert(count == 1);
}


static void testDisconnectRemovesOwnedProjectiles() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  auto& leaving = server::GameRoomTestAccess::addClient(room, 1000);
  auto& staying = server::GameRoomTestAccess::addClient(room, 1001);
  const auto leavingId = leaving.id;
  const auto leavingEndpoint = leaving.endpoint;
  const auto stayingId = staying.id;
  const auto stayingEndpoint = staying.endpoint;

  server::ServerEntity oldMissile;
  oldMissile.id = 2000;
  oldMissile.type = server::ServerEntityType::PlayerMissile;
  oldMissile.ownerId = leavingId;
  server::GameRoomTestAccess::addEntity(room, oldMissile);

  server::ServerEntity activeMissile;
  activeMissile.id = 2001;
  activeMissile.type = server::ServerEntityType::PlayerMissile;
  activeMissile.ownerId = stayingId;
  server::GameRoomTestAccess::addEntity(room, activeMissile);

  server::GameRoomTestAccess::removeClient(room, leavingId);
  assert(!room.containsEndpoint(leavingEndpoint));
  assert(room.containsEndpoint(stayingEndpoint));
  assert(server::GameRoomTestAccess::ownedMissiles(room, leavingId) == 0);
  assert(server::GameRoomTestAccess::ownedMissiles(room, stayingId) == 1);

  std::vector<network::NetworkId> destroyed;
  for (const auto& sent : socket.sent) {
    if (packetType(sent) != network::PacketType::DestroyEntity ||
        sent.endpoint.port != stayingEndpoint.port) continue;
    network::PacketStreamReader reader(sent.data);
    network::PacketHeader header{};
    network::NetworkId id{};
    reader >> header >> id;
    destroyed.push_back(id);
  }
  assert(std::find(destroyed.begin(), destroyed.end(), leavingId) !=
         destroyed.end());
  assert(std::find(destroyed.begin(), destroyed.end(), oldMissile.id) !=
         destroyed.end());
}

static void testFixedTickReportsDroppedSimulationTime() {
  GameplayMockSocket socket;
  std::uint32_t sequence = 1;
  server::GameRoom room(socket, 1, 4, 1, false, sequence);
  server::GameRoomTestAccess::addClient(room);
  room.update(2.0f);
  assert(room.droppedSimulationSeconds() >= 1.74);
  assert(room.droppedSimulationSeconds() <= 1.76);
}

int main() {
  testBonusPickupsAndAugments();
  testSharedPlayerMovement();
  testInputsAreAppliedOneStepEachAndAcknowledged();
  testInputQueueIsBounded();
  testDeadPlayerInputsAreAcknowledgedWithoutMoving();
  testBossHitboxMatchesItsSprite();
  testBossProgressionAndScore();
  testSessionValidationAndInputSequenceWrap();
  testRammingDoesNotAwardScore();
  testSnapshotDeltaAndKeyframeCadence();
  testSnapshotDeltaNamesItsBaseAndLifecycle();
  testFullSnapshotIsSentOnlyToTheRequester();
  testDisconnectRemovesOwnedProjectiles();
  testFixedTickReportsDroppedSimulationTime();
  return 0;
}
