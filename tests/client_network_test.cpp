#include "engine/core/SnapshotClock.hpp"
#include "engine/ecs/SnapshotBuffer.hpp"
#include "engine/systems/SnapshotInterpolationSystem.hpp"
#include "network/ClientNetworkManager.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

using namespace rtype;

class ClientMockSocket final : public network::INetworkSocket {
public:
  void bind(unsigned short = 0) override {}
  void setNonBlocking(bool) override {}
  unsigned short getLocalPort() const override { return 50000; }

  void send(const network::PacketData& data,
            const network::Endpoint& endpoint) override {
    sent.push_back({data, endpoint});
  }

  bool receive(network::PacketData& data,
               network::Endpoint& endpoint) override {
    if (incoming.empty()) return false;
    data = std::move(incoming.front().first);
    endpoint = std::move(incoming.front().second);
    incoming.pop_front();
    return true;
  }

  void enqueue(network::PacketData data, network::Endpoint endpoint) {
    incoming.emplace_back(std::move(data), std::move(endpoint));
  }

  std::deque<std::pair<network::PacketData, network::Endpoint>> incoming;
  std::vector<std::pair<network::PacketData, network::Endpoint>> sent;
};

static network::PacketData makePacket(network::PacketType type,
                                      std::uint32_t sequence,
                                      const network::PacketData& payload) {
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, type, sequence,
    static_cast<std::uint16_t>(payload.size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto packet = writer.getBuffer();
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

static void testReorderedConnectAndPlayerSpawn() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  std::vector<bool> localFlags;
  std::vector<network::NetworkId> spawnedIds;
  manager.setOnEntitySpawned(
    [&](engine::Entity, network::NetworkId id,
        network::ext::EntityType, bool local) {
      spawnedIds.push_back(id);
      localFlags.push_back(local);
    });
  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  const network::Endpoint server{"127.0.0.1", 4242};

  network::PacketStreamWriter spawnPayload;
  spawnPayload << network::NetworkId{1000} << 200.0f << 300.0f
               << std::uint8_t{0};
  socket.enqueue(makePacket(network::PacketType::SpawnPlayer, 2,
                            spawnPayload.getBuffer()), server);
  manager.update(0.0f);
  assert(spawnedIds.empty());

  network::PacketStreamWriter connectPayload;
  connectPayload << network::NetworkId{1000} << std::uint8_t{1}
                 << std::uint64_t{0x123456789ABCDEF0ull};
  socket.enqueue(makePacket(network::PacketType::ConnectAck, 1,
                            connectPayload.getBuffer()), server);
  manager.update(0.0f);
  assert(spawnedIds.size() == 1);
  assert(spawnedIds.front() == 1000);
  assert(localFlags.front());
}

static void testPackedDeltaSnapshotIsApplied() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::Entity remoteEntity = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId,
        network::ext::EntityType, bool) { remoteEntity = entity; });
  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  const network::Endpoint server{"127.0.0.1", 4242};

  network::PacketStreamWriter connectPayload;
  connectPayload << network::NetworkId{1000} << std::uint8_t{1}
                 << std::uint64_t{0x123456789ABCDEF0ull};
  socket.enqueue(makePacket(network::PacketType::ConnectAck, 1,
                            connectPayload.getBuffer()), server);

  network::PacketStreamWriter spawnPayload;
  spawnPayload << network::NetworkId{2000} << std::uint8_t{1}
               << 100.0f << 200.0f;
  socket.enqueue(makePacket(network::PacketType::SpawnEntity, 2,
                            spawnPayload.getBuffer()), server);
  manager.update(0.0f);
  assert(remoteEntity != 0);

  network::PacketStreamWriter deltaPayload;
  deltaPayload << std::uint32_t{7} << std::uint16_t{1}
               << network::packEntityState(network::EntityStateData{
                    2000, 456.0f, 321.0f, -10.0f, 25.0f});
  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 3,
                            deltaPayload.getBuffer()), server);
  manager.update(0.0f);
  auto* transform = registry.getComponent<engine::TransformComponent>(
    remoteEntity);
  assert(transform != nullptr);
  assert(transform->targetPosition.x == 456.0f);
  assert(transform->targetPosition.y == 321.0f);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{7});
}


static std::vector<std::uint32_t> bufferedTicks(const engine::SnapshotBufferComponent& buffer) {
  std::vector<std::uint32_t> ticks;
  for (const auto& sample : buffer.samples) ticks.push_back(sample.tick);
  return ticks;
}

static void testSnapshotBufferOrderingAndBounds() {
  engine::SnapshotBufferComponent buffer{{}, 3};
  assert(engine::pushSnapshotSample(buffer, {5, {5.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Inserted);
  assert(engine::pushSnapshotSample(buffer, {3, {3.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Inserted);
  assert(engine::pushSnapshotSample(buffer, {4, {4.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Inserted);
  assert((bufferedTicks(buffer) == std::vector<std::uint32_t>{3, 4, 5}));

  assert(engine::pushSnapshotSample(buffer, {4, {99.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Duplicate);
  assert(buffer.samples[1].position.x == 4.0f);

  assert(engine::pushSnapshotSample(buffer, {7, {7.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Inserted);
  assert((bufferedTicks(buffer) == std::vector<std::uint32_t>{4, 5, 7}));

  assert(engine::pushSnapshotSample(buffer, {6, {6.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::Inserted);
  assert((bufferedTicks(buffer) == std::vector<std::uint32_t>{5, 6, 7}));

  assert(engine::pushSnapshotSample(buffer, {2, {2.0f, 0.0f}, {}}) == engine::SnapshotInsertResult::TooOld);
  assert((bufferedTicks(buffer) == std::vector<std::uint32_t>{5, 6, 7}));
}

static network::PacketData snapshotPayload(std::uint32_t tick, network::NetworkId id, float x, float y) {
  network::PacketStreamWriter payload;
  payload << tick << std::uint16_t{1} << network::packEntityState(network::EntityStateData{id, x, y, 0.0f, 0.0f});
  return payload.getBuffer();
}

static void testLateSnapshotIsBufferedWithoutMovingTarget() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::Entity remoteEntity = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId, network::ext::EntityType, bool) { remoteEntity = entity; });
  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  const network::Endpoint server{"127.0.0.1", 4242};

  network::PacketStreamWriter connectPayload;
  connectPayload << network::NetworkId{1000} << std::uint8_t{1} << std::uint64_t{42};
  socket.enqueue(makePacket(network::PacketType::ConnectAck, 1, connectPayload.getBuffer()), server);
  network::PacketStreamWriter spawnPayload;
  spawnPayload << network::NetworkId{2000} << std::uint8_t{1} << 100.0f << 200.0f;
  socket.enqueue(makePacket(network::PacketType::SpawnEntity, 2, spawnPayload.getBuffer()), server);
  manager.update(0.0f);
  assert(remoteEntity != 0);

  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 6, snapshotPayload(12, 2000, 300.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 4, snapshotPayload(10, 2000, 200.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 5, snapshotPayload(11, 2000, 250.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 5, snapshotPayload(11, 2000, 250.0f, 200.0f)), server);
  manager.update(0.0f);

  const auto* buffer = registry.getComponent<engine::SnapshotBufferComponent>(remoteEntity);
  assert(buffer != nullptr);
  assert((bufferedTicks(*buffer) == std::vector<std::uint32_t>{10, 11, 12}));
  assert(buffer->samples.front().position.x == 200.0f);
  assert(buffer->samples.back().position.x == 300.0f);

  const auto* transform = registry.getComponent<engine::TransformComponent>(remoteEntity);
  assert(transform != nullptr);
  assert(transform->targetPosition.x == 300.0f);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{12});
}

static bool near(double value, double expected) {
  return std::abs(value - expected) < 0.001;
}

static void testSnapshotSamplingInterpolatesAndExtrapolates() {
  engine::SnapshotBufferComponent buffer{{}, 8};
  assert(!engine::sampleSnapshotPosition(buffer, 10.0, 60.0, 6.0));

  engine::pushSnapshotSample(buffer, {10, {100.0f, 50.0f}, {}});
  engine::pushSnapshotSample(buffer, {20, {200.0f, 150.0f}, {600.0f, 0.0f}});

  auto position = engine::sampleSnapshotPosition(buffer, 15.0, 60.0, 6.0);
  assert(position && near(position->x, 150.0) && near(position->y, 100.0));
  position = engine::sampleSnapshotPosition(buffer, 5.0, 60.0, 6.0);
  assert(position && near(position->x, 100.0));
  position = engine::sampleSnapshotPosition(buffer, 25.0, 60.0, 6.0);
  assert(position && near(position->x, 250.0) && near(position->y, 150.0));
  position = engine::sampleSnapshotPosition(buffer, 40.0, 60.0, 6.0);
  assert(position && near(position->x, 260.0));
}

static void testSnapshotClockFollowsServerWithDelay() {
  engine::SnapshotClock clock(60.0, 0.1, 0.1);
  assert(!clock.isSynchronized());
  clock.observe(60);
  assert(clock.isSynchronized());
  assert(near(clock.nextRenderTick(), 54.0));

  clock.advance(0.5f);
  assert(near(clock.nextRenderTick(), 84.0));

  clock.observe(85);
  const double afterLatePacket = clock.nextRenderTick();
  assert(afterLatePacket >= 84.0);

  clock.advance(1.0f / 60.0f);
  assert(clock.nextRenderTick() >= afterLatePacket);

  clock.observe(6000);
  assert(clock.nextRenderTick() > 5000.0);
}

static void testRemoteEntityAppearsWhenRenderTimeReachesIt() {
  engine::Registry registry;
  engine::SnapshotClock clock(60.0, 0.1, 0.1);
  engine::SnapshotInterpolationSystem system(clock);
  const engine::Entity ship = registry.createEntity();
  registry.addComponent(ship, engine::TransformComponent{});
  registry.addComponent(ship, engine::SpriteComponent{});
  registry.addComponent(ship, engine::SnapshotBufferComponent{{}, 8});
  const engine::Entity tag = registry.createEntity();
  registry.addComponent(tag, engine::TransformComponent{});
  registry.addComponent(tag, engine::TextComponent{});
  registry.addComponent(tag, engine::FollowComponent{ship, {}});

  system.update(registry, 0.0f);
  assert(!registry.getComponent<engine::SpriteComponent>(ship)->visible);
  assert(!registry.getComponent<engine::TextComponent>(tag)->visible);

  engine::pushSnapshotSample(*registry.getComponent<engine::SnapshotBufferComponent>(ship), {60, {300.0f, 100.0f}, {}});
  clock.observe(60);
  system.update(registry, 0.0f);
  assert(!registry.getComponent<engine::SpriteComponent>(ship)->visible);

  clock.advance(0.2f);
  system.update(registry, 0.0f);
  assert(registry.getComponent<engine::SpriteComponent>(ship)->visible);
  assert(registry.getComponent<engine::TextComponent>(tag)->visible);
  assert(near(registry.getComponent<engine::TransformComponent>(ship)->position.x, 300.0));
}

static void spawnRemoteEntity(ClientMockSocket& socket, client::ClientNetworkManager& manager) {
  const network::Endpoint server{"127.0.0.1", 4242};
  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  network::PacketStreamWriter connectPayload;
  connectPayload << network::NetworkId{1000} << std::uint8_t{1} << std::uint64_t{42};
  socket.enqueue(makePacket(network::PacketType::ConnectAck, 1, connectPayload.getBuffer()), server);
  network::PacketStreamWriter spawnPayload;
  spawnPayload << network::NetworkId{2000} << std::uint8_t{1} << 100.0f << 200.0f;
  socket.enqueue(makePacket(network::PacketType::SpawnEntity, 2, spawnPayload.getBuffer()), server);
  manager.update(0.0f);
}

static network::PacketData destroyPacket(std::uint32_t sequence, network::NetworkId id) {
  network::PacketStreamWriter payload;
  payload << id;
  return makePacket(network::PacketType::DestroyEntity, sequence, payload.getBuffer());
}

static void testRemoteDestroyWaitsForRenderTime() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::SnapshotClock clock(60.0, 0.1, 0.1);
  manager.setSnapshotClock(&clock);
  engine::Entity remoteEntity = 0;
  int destroyedCount = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId, network::ext::EntityType, bool) { remoteEntity = entity; });
  manager.setOnEntityDestroyed(
    [&](engine::Entity, network::NetworkId, network::ext::EntityType) { ++destroyedCount; });
  spawnRemoteEntity(socket, manager);
  assert(remoteEntity != 0);
  const network::Endpoint server{"127.0.0.1", 4242};

  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 3, snapshotPayload(60, 2000, 150.0f, 200.0f)), server);
  manager.update(0.0f);
  clock.nextRenderTick();

  socket.enqueue(destroyPacket(4, 2000), server);
  manager.update(0.0f);
  assert(destroyedCount == 0);
  assert(registry.getComponent<engine::TransformComponent>(remoteEntity) != nullptr);

  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 5, snapshotPayload(61, 2000, 999.0f, 200.0f)), server);
  manager.update(0.0f);
  const auto* buffer = registry.getComponent<engine::SnapshotBufferComponent>(remoteEntity);
  assert(buffer != nullptr && buffer->samples.back().tick == 60);

  clock.advance(0.2f);
  clock.nextRenderTick();
  manager.update(0.0f);
  assert(destroyedCount == 1);
  assert(registry.getComponent<engine::TransformComponent>(remoteEntity) == nullptr);
}

static void testDestroyIsImmediateWithoutSnapshotClock() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::Entity remoteEntity = 0;
  int destroyedCount = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId, network::ext::EntityType, bool) { remoteEntity = entity; });
  manager.setOnEntityDestroyed(
    [&](engine::Entity, network::NetworkId, network::ext::EntityType) { ++destroyedCount; });
  spawnRemoteEntity(socket, manager);
  socket.enqueue(destroyPacket(3, 2000), {"127.0.0.1", 4242});
  manager.update(0.0f);
  assert(destroyedCount == 1);
  assert(registry.getComponent<engine::TransformComponent>(remoteEntity) == nullptr);
}

static network::PacketData inputAckPacket(std::uint32_t sequence, std::uint32_t lastInput, float x, float y) {
  network::PacketStreamWriter payload;
  payload << lastInput << std::uint32_t{0} << x << y;
  return makePacket(network::PacketType::InputAck, sequence, payload.getBuffer());
}

static void testInputNumbersAndAcknowledgements() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  std::vector<client::ClientNetworkManager::InputAck> acks;
  manager.setOnInputAck([&](const client::ClientNetworkManager::InputAck& ack) { acks.push_back(ack); });
  assert(!manager.sendInputs(false, false, false, true, false));

  spawnRemoteEntity(socket, manager);
  const auto first = manager.sendInputs(false, false, false, true, false);
  const auto second = manager.sendInputs(false, false, false, true, false);
  assert(first && second && *second > *first);

  const network::Endpoint server{"127.0.0.1", 4242};
  socket.enqueue(inputAckPacket(10, *second, 110.0f, 200.0f), server);
  socket.enqueue(inputAckPacket(11, *first, 105.0f, 200.0f), server);
  manager.update(0.0f);
  assert(acks.size() == 1);
  assert(acks.front().lastInput == *second);
  assert(acks.front().x == 110.0f && acks.front().y == 200.0f);
}

static void testReliableStateUpdatesIgnoreReordering() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  std::vector<std::pair<int, int>> healthUpdates;
  std::vector<std::pair<std::uint8_t, int>> augmentUpdates;
  std::vector<std::uint32_t> scores;
  manager.setOnHealthChanged(
    [&](engine::Entity, network::NetworkId, network::ext::EntityType,
        int health, int, int) { healthUpdates.emplace_back(health, 0); });
  manager.setOnAugmentChanged(
    [&](engine::Entity, network::NetworkId, network::ext::EntityType,
        network::ext::AugmentId augment, int level, float) {
      augmentUpdates.emplace_back(augment, level);
    });
  manager.setOnScoreChanged(
    [&](network::NetworkId, std::uint32_t score) { scores.push_back(score); });

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  const network::Endpoint server{"127.0.0.1", 4242};

  network::PacketStreamWriter connectPayload;
  connectPayload << network::NetworkId{1000} << std::uint8_t{1}
                 << std::uint64_t{0x123456789ABCDEF0ull};
  socket.enqueue(makePacket(network::PacketType::ConnectAck, 1,
                            connectPayload.getBuffer()), server);

  network::PacketStreamWriter doubleShotPayload;
  doubleShotPayload << network::NetworkId{1000} << std::uint8_t{1}
                    << std::uint8_t{1} << 10.0f;
  socket.enqueue(makePacket(network::PacketType::AugmentUpdate, 3,
                            doubleShotPayload.getBuffer()), server);
  network::PacketStreamWriter damagePayload;
  damagePayload << network::NetworkId{1000} << std::uint8_t{3}
                << std::uint8_t{2} << 10.0f;
  socket.enqueue(makePacket(network::PacketType::AugmentUpdate, 4,
                            damagePayload.getBuffer()), server);

  network::PacketStreamWriter spawnPayload;
  spawnPayload << network::NetworkId{1000} << 200.0f << 300.0f
               << std::uint8_t{0};
  socket.enqueue(makePacket(network::PacketType::SpawnPlayer, 2,
                            spawnPayload.getBuffer()), server);
  manager.update(0.0f);
  assert(augmentUpdates.size() == 2);
  assert(augmentUpdates[0] == std::make_pair(std::uint8_t{1}, 1));
  assert(augmentUpdates[1] == std::make_pair(std::uint8_t{3}, 2));

  network::PacketStreamWriter latestHealth;
  latestHealth << network::NetworkId{1000} << std::uint8_t{2}
               << std::uint8_t{5};
  socket.enqueue(makePacket(network::PacketType::HealthUpdate, 10,
                            latestHealth.getBuffer()), server);
  network::PacketStreamWriter staleHealth;
  staleHealth << network::NetworkId{1000} << std::uint8_t{4}
              << std::uint8_t{5};
  socket.enqueue(makePacket(network::PacketType::HealthUpdate, 9,
                            staleHealth.getBuffer()), server);

  network::PacketStreamWriter latestScore;
  latestScore << network::NetworkId{1000} << std::uint32_t{900};
  socket.enqueue(makePacket(network::PacketType::ScoreUpdate, 12,
                            latestScore.getBuffer()), server);
  network::PacketStreamWriter staleScore;
  staleScore << network::NetworkId{1000} << std::uint32_t{800};
  socket.enqueue(makePacket(network::PacketType::ScoreUpdate, 11,
                            staleScore.getBuffer()), server);

  network::PacketStreamWriter clearedDoubleShot;
  clearedDoubleShot << network::NetworkId{1000} << std::uint8_t{1}
                    << std::uint8_t{0} << 0.0f;
  socket.enqueue(makePacket(network::PacketType::AugmentUpdate, 14,
                            clearedDoubleShot.getBuffer()), server);
  socket.enqueue(makePacket(network::PacketType::AugmentUpdate, 13,
                            doubleShotPayload.getBuffer()), server);
  manager.update(0.0f);

  assert(healthUpdates.size() == 1);
  assert(healthUpdates.back().first == 2);
  assert(scores.size() == 1 && scores.back() == 900);
  assert(augmentUpdates.size() == 3);
  assert(augmentUpdates.back() == std::make_pair(std::uint8_t{1}, 0));
}

int main() {
  testReorderedConnectAndPlayerSpawn();
  testPackedDeltaSnapshotIsApplied();
  testSnapshotBufferOrderingAndBounds();
  testLateSnapshotIsBufferedWithoutMovingTarget();
  testSnapshotSamplingInterpolatesAndExtrapolates();
  testSnapshotClockFollowsServerWithDelay();
  testRemoteEntityAppearsWhenRenderTimeReachesIt();
  testRemoteDestroyWaitsForRenderTime();
  testDestroyIsImmediateWithoutSnapshotClock();
  testInputNumbersAndAcknowledgements();
  testReliableStateUpdatesIgnoreReordering();
  return 0;
}
