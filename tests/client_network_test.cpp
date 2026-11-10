#include "engine/core/SnapshotClock.hpp"
#include "engine/ecs/SnapshotBuffer.hpp"
#include "engine/systems/SnapshotInterpolationSystem.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/LocalPrediction.hpp"
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
  void bind(unsigned short = 0) override { ++binds; }
  void setNonBlocking(bool) override {}
  unsigned short getLocalPort() const override { return 50000; }

  int binds{0};

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

  const network::SnapshotState base{
    {2000, network::packEntityState(network::EntityStateData{2000, 100.0f, 200.0f, 0.0f, 0.0f})}};
  network::PacketStreamWriter fullPayload;
  fullPayload << std::uint32_t{6} << std::uint16_t{1} << base.at(2000);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 3, fullPayload.getBuffer()), server);

  const network::SnapshotState next{
    {2000, network::packEntityState(network::EntityStateData{2000, 456.0f, 321.0f, -10.0f, 25.0f})}};
  network::PacketStreamWriter deltaPayload;
  network::writeSnapshotDelta(deltaPayload, network::makeSnapshotDelta(base, 6, next, 7));
  socket.enqueue(makePacket(network::PacketType::GameStateSyncDelta, 4,
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

  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 6, snapshotPayload(12, 2000, 300.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 4, snapshotPayload(10, 2000, 200.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 5, snapshotPayload(11, 2000, 250.0f, 200.0f)), server);
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 5, snapshotPayload(11, 2000, 250.0f, 200.0f)), server);
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

  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 3, snapshotPayload(60, 2000, 150.0f, 200.0f)), server);
  manager.update(0.0f);
  clock.nextRenderTick();

  socket.enqueue(destroyPacket(4, 2000), server);
  manager.update(0.0f);
  assert(destroyedCount == 0);
  assert(registry.getComponent<engine::TransformComponent>(remoteEntity) != nullptr);

  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 5, snapshotPayload(61, 2000, 999.0f, 200.0f)), server);
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

static void testPredictionMatchesServerAndReplaysPendingInputs() {
  client::LocalPrediction prediction;
  const gameplay::MovementInput right{false, false, false, true};
  const float step = gameplay::PLAYER_SPEED * gameplay::INPUT_STEP_SECONDS;
  prediction.applyInput(1, right);
  assert(prediction.pendingCount() == 0);

  prediction.reset({100.0f, 100.0f});
  prediction.applyInput(1, right);
  prediction.applyInput(2, right);
  prediction.applyInput(3, right);
  assert(near(prediction.predicted().x, 100.0 + 3.0 * step));
  assert(prediction.pendingCount() == 3);

  const auto confirmed = gameplay::applyMovement({100.0f, 100.0f}, right, gameplay::INPUT_STEP_SECONDS);
  prediction.reconcile(1, confirmed);
  assert(prediction.pendingCount() == 2);
  assert(near(prediction.predicted().x, 100.0 + 3.0 * step));
  assert(near(prediction.lastCorrection(), 0.0));
  assert(near(prediction.displayed().x, prediction.predicted().x));
}

static void testPredictionCorrectionIsSmoothedOrSnapped() {
  client::LocalPrediction prediction;
  prediction.configure({120, 12.0f, 120.0f, {}});
  const gameplay::MovementInput right{false, false, false, true};
  const float step = gameplay::PLAYER_SPEED * gameplay::INPUT_STEP_SECONDS;
  prediction.reset({100.0f, 100.0f});
  prediction.applyInput(1, right);
  prediction.applyInput(2, right);

  const auto shownBefore = prediction.displayed();
  prediction.reconcile(1, {100.0f + step, 130.0f});
  assert(near(prediction.lastCorrection(), 30.0));
  assert(near(prediction.predicted().x, 100.0 + 2.0 * step) && near(prediction.predicted().y, 130.0));
  assert(near(prediction.displayed().x, shownBefore.x) && near(prediction.displayed().y, shownBefore.y));

  prediction.update(0.1f);
  assert(prediction.displayed().y > 100.0f && prediction.displayed().y < 130.0f);
  prediction.update(2.0f);
  assert(near(prediction.displayed().y, 130.0));

  prediction.reconcile(2, {800.0f, 600.0f});
  assert(prediction.lastCorrection() > 120.0f);
  assert(near(prediction.displayed().x, 800.0) && near(prediction.displayed().y, 600.0));
  assert(prediction.pendingCount() == 0);
}

static void testPredictionHandlesSequenceWrapAndStop() {
  client::LocalPrediction prediction;
  const gameplay::MovementInput down{false, true, false, false};
  prediction.reset({50.0f, 50.0f});
  prediction.applyInput(0xFFFFFFFEu, down);
  prediction.applyInput(0xFFFFFFFFu, down);
  prediction.applyInput(1u, down);
  prediction.reconcile(0xFFFFFFFFu, {50.0f, 60.0f});
  assert(prediction.pendingCount() == 1);
  assert(near(prediction.predicted().y, 65.0));

  prediction.stop();
  assert(!prediction.active());
  prediction.applyInput(2u, down);
  prediction.reconcile(2u, {0.0f, 0.0f});
  assert(prediction.pendingCount() == 0);
  assert(near(prediction.predicted().y, 65.0));
}

static network::PacketData fullPacket(std::uint32_t sequence, std::uint32_t tick, const network::SnapshotState& state) {
  network::PacketStreamWriter payload;
  payload << tick << static_cast<std::uint16_t>(state.size());
  for (const auto& [id, packed] : state) payload << packed;
  return makePacket(network::PacketType::GameStateSyncPacked, sequence, payload.getBuffer());
}

static network::PacketData deltaPacket(std::uint32_t sequence, const network::SnapshotState& base,
                                       std::uint32_t baseTick, const network::SnapshotState& next, std::uint32_t tick) {
  network::PacketStreamWriter payload;
  network::writeSnapshotDelta(payload, network::makeSnapshotDelta(base, baseTick, next, tick));
  return makePacket(network::PacketType::GameStateSyncDelta, sequence, payload.getBuffer());
}

static network::SnapshotState stateAt(network::NetworkId id, float x) {
  return {{id, network::packEntityState(network::EntityStateData{id, x, 200.0f, 0.0f, 0.0f})}};
}

static void testDeltaNeedsAKnownBase() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::Entity remoteEntity = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId, network::ext::EntityType, bool) { remoteEntity = entity; });
  spawnRemoteEntity(socket, manager);
  const network::Endpoint server{"127.0.0.1", 4242};
  auto* transform = registry.getComponent<engine::TransformComponent>(remoteEntity);

  socket.enqueue(deltaPacket(3, stateAt(2000, 100.0f), 9, stateAt(2000, 300.0f), 10), server);
  manager.update(0.0f);
  assert(manager.getUnknownBaseDeltaCount() == 1);
  assert(transform->targetPosition.x == 100.0f);
  assert(!manager.getLastSnapshotTick());

  socket.enqueue(fullPacket(4, 20, stateAt(2000, 400.0f)), server);
  socket.enqueue(deltaPacket(5, stateAt(2000, 400.0f), 20, stateAt(2000, 410.0f), 21), server);
  manager.update(0.0f);
  assert(manager.getUnknownBaseDeltaCount() == 1);
  assert(transform->targetPosition.x == 410.0f);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{21});

  network::SnapshotState withMissile = stateAt(2000, 420.0f);
  withMissile.emplace(2001, network::packEntityState(network::EntityStateData{2001, 50.0f, 50.0f, 900.0f, 0.0f}));
  socket.enqueue(deltaPacket(6, stateAt(2000, 410.0f), 21, withMissile, 22), server);
  socket.enqueue(deltaPacket(7, withMissile, 22, stateAt(2000, 430.0f), 23), server);
  socket.enqueue(deltaPacket(8, stateAt(2000, 430.0f), 23, stateAt(2000, 440.0f), 24), server);
  manager.update(0.0f);
  assert(manager.getUnknownBaseDeltaCount() == 1);
  assert(transform->targetPosition.x == 440.0f);
}

static std::size_t sentOfType(const ClientMockSocket& socket, network::PacketType type) {
  std::size_t count = 0;
  for (const auto& [data, endpoint] : socket.sent) {
    network::PacketStreamReader reader(data);
    network::PacketHeader header{};
    reader >> header;
    if (header.type == type) ++count;
  }
  return count;
}

static void testEarlyDeltaWaitsForItsBase() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::Entity remoteEntity = 0;
  manager.setOnEntitySpawned(
    [&](engine::Entity entity, network::NetworkId, network::ext::EntityType, bool) { remoteEntity = entity; });
  spawnRemoteEntity(socket, manager);
  const network::Endpoint server{"127.0.0.1", 4242};

  socket.enqueue(fullPacket(3, 5, stateAt(2000, 100.0f)), server);
  socket.enqueue(deltaPacket(5, stateAt(2000, 110.0f), 6, stateAt(2000, 120.0f), 7), server);
  socket.enqueue(deltaPacket(4, stateAt(2000, 100.0f), 5, stateAt(2000, 110.0f), 6), server);
  manager.update(0.016f);
  assert(manager.getUnknownBaseDeltaCount() == 1);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{7});
  assert(registry.getComponent<engine::TransformComponent>(remoteEntity)->targetPosition.x == 120.0f);
  const auto* buffer = registry.getComponent<engine::SnapshotBufferComponent>(remoteEntity);
  assert((bufferedTicks(*buffer) == std::vector<std::uint32_t>{5, 6, 7}));

  manager.update(0.2f);
  assert(manager.getFullSnapshotRequestCount() == 0);
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 0);
}

static void testLostBaseRequestsFullSnapshot() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  spawnRemoteEntity(socket, manager);
  const network::Endpoint server{"127.0.0.1", 4242};

  socket.enqueue(fullPacket(3, 5, stateAt(2000, 100.0f)), server);
  socket.enqueue(deltaPacket(4, stateAt(2000, 110.0f), 6, stateAt(2000, 120.0f), 7), server);
  manager.update(0.05f);
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 0);

  manager.update(0.06f);
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 1);
  const auto& request = socket.sent.back().first;
  network::PacketStreamReader reader(request);
  network::PacketHeader header{};
  network::NetworkId id{};
  std::uint64_t token{};
  reader >> header >> id >> token;
  assert(header.type == network::PacketType::SnapshotRequest);
  assert(id == 1000 && token == 42);

  socket.enqueue(deltaPacket(5, stateAt(2000, 130.0f), 8, stateAt(2000, 140.0f), 9), server);
  manager.update(0.11f);
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 1);

  socket.enqueue(deltaPacket(6, stateAt(2000, 150.0f), 10, stateAt(2000, 160.0f), 11), server);
  manager.update(0.11f);
  manager.update(0.11f);
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 2);

  socket.enqueue(fullPacket(7, 12, stateAt(2000, 170.0f)), server);
  socket.enqueue(deltaPacket(8, stateAt(2000, 170.0f), 12, stateAt(2000, 180.0f), 13), server);
  manager.update(0.2f);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{13});
  assert(sentOfType(socket, network::PacketType::SnapshotRequest) == 2);
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

using SessionState = client::ClientNetworkManager::SessionState;
using DisconnectReason = client::ClientNetworkManager::DisconnectReason;

static network::PacketData connectAckPacket(std::uint32_t sequence, std::uint64_t token) {
  network::PacketStreamWriter payload;
  payload << network::NetworkId{1000} << std::uint8_t{1} << token;
  return makePacket(network::PacketType::ConnectAck, sequence, payload.getBuffer());
}

static std::size_t countSent(const ClientMockSocket& socket, network::PacketType type) {
  std::size_t count = 0;
  for (const auto& [data, endpoint] : socket.sent)
    if (static_cast<network::PacketType>(data.at(2)) == type) ++count;
  return count;
}

static void testSessionStatesFollowTheConnection() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  assert(manager.getState() == SessionState::Disconnected);
  assert(manager.getDisconnectReason() == DisconnectReason::None);

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  assert(manager.getState() == SessionState::Connecting);
  assert(!manager.isConnected());

  socket.enqueue(connectAckPacket(1, 0x1122334455667788ull), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::InGame);
  assert(manager.isConnected());

  manager.disconnect();
  assert(manager.getState() == SessionState::Disconnected);
  assert(manager.getDisconnectReason() == DisconnectReason::Left);
  assert(countSent(socket, network::PacketType::Disconnect) == 1);

  manager.disconnect();
  assert(countSent(socket, network::PacketType::Disconnect) == 1);
  socket.enqueue(connectAckPacket(2, 0x1122334455667788ull), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::Disconnected);
  assert(!manager.sendInputs(false, false, false, false, false));
}

static void testConnectRejectEndsTheSession() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  network::PacketStreamWriter reason;
  reason << std::uint8_t{1};
  socket.enqueue(makePacket(network::PacketType::ConnectReject, 1, reason.getBuffer()), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::Disconnected);
  assert(manager.getDisconnectReason() == DisconnectReason::Rejected);

  const std::size_t sent = countSent(socket, network::PacketType::ConnectReq);
  for (int i = 0; i < 4; ++i) manager.update(0.5f);
  assert(countSent(socket, network::PacketType::ConnectReq) == sent);
}

static void testConnectTimesOutAfterEveryRetry() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  for (int i = 0; i < 8; ++i) {
    manager.update(0.5f);
    assert(manager.getState() == SessionState::Connecting);
  }
  assert(countSent(socket, network::PacketType::ConnectReq) == 9);

  manager.update(0.5f);
  assert(manager.getState() == SessionState::Disconnected);
  assert(manager.getDisconnectReason() == DisconnectReason::TimedOut);
  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::ConnectReq) == 9);
}

static void testLobbyOpensTheSocketWithoutConnecting() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};

  manager.openLobby("127.0.0.1", 4242);
  assert(manager.getState() == SessionState::Lobby);
  assert(socket.binds == 1);
  assert(socket.sent.empty());
  manager.update(0.5f);
  assert(socket.sent.size() == countSent(socket, network::PacketType::Ping));

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  assert(manager.getState() == SessionState::Connecting);
  assert(socket.binds == 1);
  assert(countSent(socket, network::PacketType::ConnectReq) == 1);

  socket.enqueue(connectAckPacket(1, 0x0102030405060708ull), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::InGame);
  manager.openLobby("127.0.0.1", 4242);
  assert(manager.getState() == SessionState::InGame);

  manager.disconnect();
  manager.openLobby("127.0.0.1", 4242);
  assert(manager.getState() == SessionState::Lobby);
  assert(manager.getDisconnectReason() == DisconnectReason::None);
  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  assert(socket.binds == 1);

  ClientMockSocket lobbySocket;
  client::ClientNetworkManager lobby(lobbySocket, registry);
  lobby.openLobby("127.0.0.1", 4242);
  lobby.disconnect();
  assert(lobby.getState() == SessionState::Disconnected);
  assert(lobby.getDisconnectReason() == DisconnectReason::Left);
  assert(lobbySocket.sent.empty());
}

using RoomResult = client::ClientNetworkManager::RoomResult;
using RoomInfo = client::ClientNetworkManager::RoomInfo;

static network::PacketData roomResultPacket(network::PacketType type, std::uint32_t sequence,
                                            std::uint32_t roomId, std::uint8_t status) {
  network::PacketStreamWriter payload;
  payload << roomId << status;
  return makePacket(type, sequence, payload.getBuffer());
}

static network::PacketData lastSent(const ClientMockSocket& socket, network::PacketType type) {
  for (auto it = socket.sent.rbegin(); it != socket.sent.rend(); ++it)
    if (static_cast<network::PacketType>(it->first.at(2)) == type) return it->first;
  return {};
}

static void testRoomListIsRequestedAndRead() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<std::pair<RoomResult, std::vector<RoomInfo>>> answers;
  manager.setOnRoomList([&](RoomResult result, const std::vector<RoomInfo>& rooms) {
    answers.emplace_back(result, rooms);
  });

  assert(!manager.requestRooms());
  manager.openLobby("127.0.0.1", 4242);
  assert(manager.requestRooms());
  const auto request = lastSent(socket, network::PacketType::RoomListReq);
  assert(request.size() == sizeof(network::PacketHeader));

  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomListReq) == 2);
  assert(lastSent(socket, network::PacketType::RoomListReq) == request);

  network::PacketStreamWriter payload;
  payload << std::uint16_t{2}
          << std::uint32_t{1} << std::uint8_t{3} << std::uint8_t{4} << std::uint8_t{1} << std::uint8_t{1}
          << std::uint32_t{7} << std::uint8_t{2} << std::uint8_t{2} << std::uint8_t{2} << std::uint8_t{2};
  socket.enqueue(makePacket(network::PacketType::RoomListAck, 30, payload.getBuffer()), server);
  manager.update(0.0f);
  assert(countSent(socket, network::PacketType::Ack) == 1);
  assert(answers.size() == 1 && answers[0].first == RoomResult::Success);
  assert(answers[0].second.size() == 2);
  assert(answers[0].second[0].id == 1 && answers[0].second[0].players == 3);
  assert(answers[0].second[0].capacity == 4 && answers[0].second[0].state == 1);
  assert(answers[0].second[1].id == 7 && answers[0].second[1].difficulty == 2);

  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomListReq) == 2);

  network::PacketStreamWriter truncated;
  truncated << std::uint16_t{1} << std::uint32_t{1};
  manager.requestRooms();
  socket.enqueue(makePacket(network::PacketType::RoomListAck, 31, truncated.getBuffer()), server);
  manager.update(0.0f);
  assert(answers.size() == 1);

  for (int i = 0; i < 9; ++i) manager.update(0.5f);
  assert(answers.size() == 2 && answers[1].first == RoomResult::TimedOut);
  assert(answers[1].second.empty());
}

static void testRoomCreationResults() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<std::pair<RoomResult, std::uint32_t>> answers;
  manager.setOnRoomCreated([&](RoomResult result, std::uint32_t roomId) { answers.emplace_back(result, roomId); });

  assert(!manager.createRoom(2, 2));
  manager.openLobby("127.0.0.1", 4242);
  assert(manager.createRoom(2, 2));
  const auto request = lastSent(socket, network::PacketType::RoomCreateReq);
  assert(request.size() == sizeof(network::PacketHeader) + 2);
  assert(request[9] == 2 && request[10] == 2);

  socket.enqueue(roomResultPacket(network::PacketType::RoomCreateAck, 40, 5, 0), server);
  manager.update(0.0f);
  assert(answers.size() == 1 && answers[0] == std::make_pair(RoomResult::Success, std::uint32_t{5}));

  socket.enqueue(roomResultPacket(network::PacketType::RoomCreateAck, 41, 6, 0), server);
  manager.update(0.0f);
  assert(answers.size() == 1);

  manager.createRoom(9, 0);
  socket.enqueue(roomResultPacket(network::PacketType::RoomCreateAck, 42, 0, 1), server);
  manager.update(0.0f);
  assert(answers.back() == std::make_pair(RoomResult::InvalidSettings, std::uint32_t{0}));

  manager.createRoom(2, 0);
  socket.enqueue(roomResultPacket(network::PacketType::RoomCreateAck, 43, 0, 2), server);
  manager.update(0.0f);
  assert(answers.back() == std::make_pair(RoomResult::LimitReached, std::uint32_t{0}));
  assert(manager.getState() == SessionState::Lobby);
}

static void testJoinRoomConnectsToTheGame() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<std::pair<RoomResult, std::uint32_t>> answers;
  manager.setOnRoomJoined([&](RoomResult result, std::uint32_t roomId) { answers.emplace_back(result, roomId); });

  manager.openLobby("127.0.0.1", 4242);
  manager.requestRooms();
  assert(manager.joinRoom("127.0.0.1", 4242, 5, 1, 0));
  assert(manager.getState() == SessionState::Connecting);
  assert(socket.binds == 1);
  const auto request = lastSent(socket, network::PacketType::RoomJoinReq);
  assert(request.size() == sizeof(network::PacketHeader) + 10);
  assert(request[9] == 5 && request[13] == 1 && request[14] == 0);
  assert(request[15] == 0 && request[16] == 0 && request[17] == 0 && request[18] == 0);

  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomJoinReq) == 2);
  assert(countSent(socket, network::PacketType::RoomListReq) == 1);

  socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, 50, 9, 0), server);
  manager.update(0.0f);
  assert(answers.empty());

  socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, 51, 5, 0), server);
  socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, 52, 5, 0), server);
  manager.update(0.0f);
  assert(answers.size() == 1 && answers[0] == std::make_pair(RoomResult::Success, std::uint32_t{5}));
  assert(manager.getState() == SessionState::Connecting);

  socket.enqueue(connectAckPacket(53, 0x0A0B0C0D0E0F1011ull), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::InGame);
  const std::size_t joins = countSent(socket, network::PacketType::RoomJoinReq);
  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomJoinReq) == joins);
  assert(!manager.joinRoom("127.0.0.1", 4242, 6));
}

static void testJoinRoomFailuresReturnToTheLobby() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<std::pair<RoomResult, std::uint32_t>> answers;
  manager.setOnRoomJoined([&](RoomResult result, std::uint32_t roomId) { answers.emplace_back(result, roomId); });

  const std::pair<std::uint8_t, RoomResult> cases[] = {
    {1, RoomResult::NotFound}, {2, RoomResult::Full}, {3, RoomResult::AlreadyInRoom}
  };
  std::uint32_t sequence = 60;
  for (const auto& [status, expected] : cases) {
    manager.openLobby("127.0.0.1", 4242);
    manager.joinRoom("127.0.0.1", 4242, 8);
    socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, sequence++, 8, status), server);
    manager.update(0.0f);
    assert(answers.back() == std::make_pair(expected, std::uint32_t{8}));
    assert(manager.getState() == SessionState::Lobby);
    const std::size_t joins = countSent(socket, network::PacketType::RoomJoinReq);
    manager.update(0.5f);
    assert(countSent(socket, network::PacketType::RoomJoinReq) == joins);
  }

  manager.joinRoom("127.0.0.1", 4242, 8);
  for (int i = 0; i < 9; ++i) manager.update(0.5f);
  assert(answers.back() == std::make_pair(RoomResult::TimedOut, std::uint32_t{8}));
  assert(manager.getState() == SessionState::Disconnected);
  assert(manager.getDisconnectReason() == DisconnectReason::TimedOut);
}

static std::size_t countRemoteEntities(engine::Registry& registry) {
  std::size_t count = 0;
  registry.each<engine::NetworkIdComponent>([&](engine::Entity, engine::NetworkIdComponent&) { ++count; });
  return count;
}

static network::PacketData spawnEntityPacket(std::uint32_t sequence, network::NetworkId netId) {
  network::PacketStreamWriter payload;
  payload << netId << std::uint8_t{1} << 500.0f << 300.0f;
  return makePacket(network::PacketType::SpawnEntity, sequence, payload.getBuffer());
}

static void joinAndConnect(client::ClientNetworkManager& manager, ClientMockSocket& socket,
                           std::uint32_t roomId, std::uint32_t sequence, std::uint64_t token) {
  const network::Endpoint server{"127.0.0.1", 4242};
  assert(manager.joinRoom("127.0.0.1", 4242, roomId));
  socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, sequence, roomId, 0), server);
  socket.enqueue(connectAckPacket(sequence + 1, token), server);
  manager.update(0.0f);
  assert(manager.getState() == SessionState::InGame);
}

static void testLeaveRoomCleansTheSession() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  engine::SnapshotClock clock(60.0, 0.1, 0.1);
  manager.setSnapshotClock(&clock);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<std::pair<RoomResult, std::uint32_t>> left;
  std::size_t destroyCallbacks = 0;
  manager.setOnRoomLeft([&](RoomResult result, std::uint32_t roomId) { left.emplace_back(result, roomId); });
  manager.setOnEntityDestroyed([&](engine::Entity, network::NetworkId, network::ext::EntityType) {
    ++destroyCallbacks;
  });

  assert(!manager.leaveRoom());
  manager.openLobby("127.0.0.1", 4242);
  assert(!manager.leaveRoom());
  joinAndConnect(manager, socket, 5, 100, 0x1111222233334444ull);

  socket.enqueue(spawnEntityPacket(102, 2000), server);
  socket.enqueue(spawnEntityPacket(103, 2001), server);
  network::PacketStreamWriter full;
  full << std::uint32_t{900} << std::uint16_t{1}
       << network::packEntityState({2000, 500.0f, 300.0f, 0.0f, 0.0f});
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 104, full.getBuffer()), server);
  manager.update(0.0f);
  assert(countRemoteEntities(registry) == 2);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{900});
  assert(clock.isSynchronized());

  assert(manager.leaveRoom());
  assert(manager.getState() == SessionState::Lobby);
  assert(manager.isLeaving());
  assert(countRemoteEntities(registry) == 0);
  assert(destroyCallbacks == 0);
  assert(!manager.getLastSnapshotTick());
  assert(!clock.isSynchronized());
  assert(manager.getLocalNetId() == 0);
  assert(!manager.sendInputs(false, false, false, true, false));
  const auto request = lastSent(socket, network::PacketType::RoomLeaveReq);
  assert(request.size() == sizeof(network::PacketHeader) + 12);
  assert(request[9] == 0xE8 && request[10] == 0x03);
  assert(request[13] == 0x44 && request[20] == 0x11);

  const std::size_t acks = countSent(socket, network::PacketType::Ack);
  socket.enqueue(spawnEntityPacket(105, 2002), server);
  manager.update(0.0f);
  assert(countRemoteEntities(registry) == 0);
  assert(countSent(socket, network::PacketType::Ack) == acks + 1);

  assert(!manager.joinRoom("127.0.0.1", 4242, 6));
  assert(!manager.connectToServer("127.0.0.1", 4242));
  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomLeaveReq) == 2);

  socket.enqueue(roomResultPacket(network::PacketType::RoomLeaveAck, 106, 5, 0), server);
  manager.update(0.0f);
  assert(left.size() == 1 && left[0] == std::make_pair(RoomResult::Success, std::uint32_t{5}));
  assert(!manager.isLeaving());
  manager.update(0.5f);
  assert(countSent(socket, network::PacketType::RoomLeaveReq) == 2);

  joinAndConnect(manager, socket, 6, 102, 0x5555666677778888ull);
  socket.enqueue(spawnEntityPacket(104, 2000), server);
  network::PacketStreamWriter restarted;
  restarted << std::uint32_t{3} << std::uint16_t{1}
            << network::packEntityState({2000, 700.0f, 200.0f, 0.0f, 0.0f});
  socket.enqueue(makePacket(network::PacketType::GameStateSyncPacked, 105, restarted.getBuffer()), server);
  manager.update(0.0f);
  assert(countRemoteEntities(registry) == 1);
  assert(manager.getLastSnapshotTick() == std::optional<std::uint32_t>{3});
  assert(socket.binds == 1);
}

static void testLeaveRoomTimesOutAndDisconnectCleans() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  std::vector<RoomResult> left;
  manager.setOnRoomLeft([&](RoomResult result, std::uint32_t) { left.push_back(result); });

  manager.openLobby("127.0.0.1", 4242);
  joinAndConnect(manager, socket, 5, 10, 0x0102030405060708ull);
  manager.leaveRoom();
  for (int i = 0; i < 9; ++i) manager.update(0.5f);
  assert(left.size() == 1 && left[0] == RoomResult::TimedOut);
  assert(countSent(socket, network::PacketType::RoomLeaveReq) == 9);
  assert(!manager.isLeaving());
  assert(manager.getState() == SessionState::Lobby);

  joinAndConnect(manager, socket, 5, 20, 0x0102030405060709ull);
  socket.enqueue(spawnEntityPacket(22, 2000), server);
  manager.update(0.0f);
  assert(countRemoteEntities(registry) == 1);
  manager.disconnect();
  assert(countRemoteEntities(registry) == 0);
  assert(manager.getState() == SessionState::Disconnected);

  ClientMockSocket connecting;
  client::ClientNetworkManager pending(connecting, registry);
  pending.openLobby("127.0.0.1", 4242);
  pending.joinRoom("127.0.0.1", 4242, 5);
  assert(pending.leaveRoom());
  assert(pending.getState() == SessionState::Lobby);
  assert(countSent(connecting, network::PacketType::RoomLeaveReq) == 0);
  const std::size_t joins = countSent(connecting, network::PacketType::RoomJoinReq);
  pending.update(0.5f);
  assert(countSent(connecting, network::PacketType::RoomJoinReq) == joins);
}

static void testNetStatsStartEmpty() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const client::NetStats& stats = manager.getStats();
  assert(stats.packetsSent == 0 && stats.packetsReceived == 0);
  assert(stats.bytesSent == 0 && stats.bytesReceived == 0);
  assert(stats.rejectedTotal() == 0);
  assert(!stats.rttMs && !stats.jitterMs && !stats.lossPercent);
  assert(stats.retransmissionsSent == 0 && stats.retransmissionsReceived == 0);

  client::NetStats counted;
  counted.reject(client::RejectReason::Duplicate);
  counted.reject(client::RejectReason::Duplicate);
  counted.reject(client::RejectReason::WrongSender);
  assert(counted.rejectedFor(client::RejectReason::Duplicate) == 2);
  assert(counted.rejectedFor(client::RejectReason::WrongSender) == 1);
  assert(counted.rejectedFor(client::RejectReason::Stale) == 0);
  assert(counted.rejectedTotal() == 3);
}

static void testNetStatsCountPacketsAndBytes() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  assert(stats.packetsSent == 1);
  assert(stats.bytesSent == sizeof(network::PacketHeader) + 2);

  manager.update(0.5f);
  assert(stats.packetsSent == 2);
  assert(stats.bytesSent == 2 * (sizeof(network::PacketHeader) + 2));

  socket.enqueue(connectAckPacket(1, 0x0102030405060708ull), server);
  manager.update(0.0f);
  assert(stats.packetsReceived == 1);
  assert(stats.bytesReceived == sizeof(network::PacketHeader) + 13);
  assert(stats.packetsSent == 3);
  assert(stats.bytesSent == 2 * (sizeof(network::PacketHeader) + 2) + sizeof(network::PacketHeader) + 4);

  manager.sendInputs(false, false, false, true, false);
  assert(stats.packetsSent == 4);
  assert(stats.bytesSent == 2 * (sizeof(network::PacketHeader) + 2) + sizeof(network::PacketHeader) + 4 +
                            sizeof(network::PacketHeader) + 17);

  socket.enqueue(connectAckPacket(2, 0x0102030405060708ull), network::Endpoint{"127.0.0.1", 5000});
  socket.enqueue(network::PacketData{0x01, 0x02, 0x03}, server);
  manager.update(0.0f);
  assert(stats.packetsReceived == 3);
  assert(stats.bytesReceived == sizeof(network::PacketHeader) + 13 + sizeof(network::PacketHeader) + 13 + 3);
  assert(stats.packetsSent == 5);
  assert(countSent(socket, network::PacketType::Ping) == 1);

  assert(stats.packetsSent == socket.sent.size());
}

static void testNetStatsCountRejectedPacketsByReason() {
  using client::RejectReason;
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();

  manager.openLobby("127.0.0.1", 4242);
  socket.enqueue(spawnEntityPacket(1, 2000), server);
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::OutsideRoom) == 1);

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  socket.enqueue(connectAckPacket(2, 0), server);
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::Malformed) == 1);
  socket.enqueue(connectAckPacket(3, 0x0102030405060708ull), server);
  manager.update(0.0f);
  assert(manager.isConnected());

  socket.enqueue(spawnEntityPacket(10, 2000), network::Endpoint{"127.0.0.1", 5000});
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::WrongSender) == 1);

  auto badMagic = spawnEntityPacket(11, 2000);
  badMagic[0] = 0x00;
  auto badSize = spawnEntityPacket(12, 2000);
  badSize.push_back(0x00);
  network::PacketStreamWriter shortHealth;
  shortHealth << network::NetworkId{2000};
  socket.enqueue(network::PacketData{0x54, 0x52, 0x13}, server);
  socket.enqueue(badMagic, server);
  socket.enqueue(badSize, server);
  socket.enqueue(makePacket(network::PacketType::HealthUpdate, 13, shortHealth.getBuffer()), server);
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::Malformed) == 5);

  socket.enqueue(spawnEntityPacket(20, 2000), server);
  socket.enqueue(spawnEntityPacket(20, 2000), server);
  socket.enqueue(spawnEntityPacket(21, 2000), server);
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::Duplicate) == 2);

  network::PacketStreamWriter score;
  score << network::NetworkId{1000} << std::uint32_t{500};
  socket.enqueue(makePacket(network::PacketType::ScoreUpdate, 31, score.getBuffer()), server);
  socket.enqueue(makePacket(network::PacketType::ScoreUpdate, 30, score.getBuffer()), server);
  network::PacketStreamWriter inputAck;
  inputAck << std::uint32_t{0} << std::uint32_t{1} << 200.0f << 300.0f;
  manager.sendInputs(false, false, false, false, false);
  network::PacketStreamWriter newerAck;
  newerAck << std::uint32_t{100} << std::uint32_t{2} << 200.0f << 300.0f;
  socket.enqueue(makePacket(network::PacketType::InputAck, 32, newerAck.getBuffer()), server);
  socket.enqueue(makePacket(network::PacketType::InputAck, 33, inputAck.getBuffer()), server);
  manager.update(0.0f);
  assert(stats.rejectedFor(RejectReason::Stale) == 2);

  assert(stats.rejectedTotal() == 11);
  assert(manager.getDroppedPacketCount() == 11);
  assert(stats.packetsReceived == 15);
}

static void testNetStatsMeasureThroughputInBytesPerSecond() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();
  const double header = sizeof(network::PacketHeader);
  const double connect = header + 2;
  const double ack = header + 4;
  const double input = header + 17;
  const double ping = header + 8;

  manager.connectToServer("127.0.0.1", 4242, 1, 0);
  socket.enqueue(connectAckPacket(1, 0x0102030405060708ull), server);
  manager.update(0.5f);
  assert(stats.bytesSentPerSecond == 0.0 && stats.bytesReceivedPerSecond == 0.0);
  manager.update(0.5f);
  assert(stats.bytesSentPerSecond == 2.0 * connect + ack + ping);
  assert(stats.bytesReceivedPerSecond == header + 13);

  for (int i = 0; i < 10; ++i) manager.sendInputs(false, false, false, true, false);
  manager.update(0.5f);
  assert(stats.bytesSentPerSecond == 2.0 * connect + ack + ping);
  manager.update(0.5f);
  assert(stats.bytesSentPerSecond == 10.0 * input + 2.0 * ping);
  assert(stats.bytesReceivedPerSecond == 0.0);

  for (int i = 0; i < 6; ++i) manager.sendInputs(false, false, false, true, false);
  manager.update(2.0f);
  assert(stats.bytesSentPerSecond == (6.0 * input + ping) / 2.0);

  manager.update(0.0f);
  manager.update(-1.0f);
  assert(stats.bytesSentPerSecond == (6.0 * input + ping) / 2.0);
}

static std::uint64_t pingTimestamp(const network::PacketData& packet) {
  network::PacketStreamReader reader(packet);
  network::PacketHeader header{};
  std::uint64_t timestamp{0};
  reader >> header >> timestamp;
  return timestamp;
}

static network::PacketData pongPacket(std::uint32_t sequence, std::uint64_t timestamp) {
  network::PacketStreamWriter payload;
  payload << timestamp;
  return makePacket(network::PacketType::Pong, sequence, payload.getBuffer());
}

static void testPingsMeasureRoundTripTime() {
  using client::RejectReason;
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();

  manager.update(1.0f);
  assert(stats.pingsSent == 0);

  manager.openLobby("127.0.0.1", 4242);
  manager.update(0.25f);
  assert(stats.pingsSent == 1);
  const auto ping = lastSent(socket, network::PacketType::Ping);
  assert(ping.size() == sizeof(network::PacketHeader) + 8);
  const std::uint64_t timestamp = pingTimestamp(ping);
  assert(timestamp == 1250000);
  assert(!stats.lastRttMs);

  socket.enqueue(pongPacket(1, timestamp), server);
  manager.update(0.04f);
  assert(stats.pongsReceived == 1);
  assert(stats.lastRttMs && std::abs(*stats.lastRttMs - 40.0) < 0.01);

  socket.enqueue(pongPacket(2, timestamp), server);
  socket.enqueue(pongPacket(3, 42), server);
  network::PacketStreamWriter shortPong;
  shortPong << std::uint32_t{7};
  socket.enqueue(makePacket(network::PacketType::Pong, 4, shortPong.getBuffer()), server);
  manager.update(0.0f);
  assert(stats.pongsReceived == 1);
  assert(stats.rejectedFor(RejectReason::Stale) == 2);
  assert(stats.rejectedFor(RejectReason::Malformed) == 1);

  manager.update(0.46f);
  assert(stats.pingsSent == 2);
  manager.update(0.25f);
  assert(stats.pingsSent == 2);
  manager.update(0.25f);
  assert(stats.pingsSent == 3);

  manager.disconnect();
  manager.update(1.0f);
  assert(stats.pingsSent == 3);
}

static void answerNextPing(client::ClientNetworkManager& manager, ClientMockSocket& socket,
                           std::uint32_t sequence, float delay) {
  const network::Endpoint server{"127.0.0.1", 4242};
  socket.sent.clear();
  manager.update(0.5f);
  const auto ping = lastSent(socket, network::PacketType::Ping);
  assert(!ping.empty());
  socket.enqueue(pongPacket(sequence, pingTimestamp(ping)), server);
  manager.update(delay);
}

static void testRoundTripTimeIsSmoothedWithJitter() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const client::NetStats& stats = manager.getStats();
  manager.openLobby("127.0.0.1", 4242);
  manager.update(0.0f);

  answerNextPing(manager, socket, 1, 0.040f);
  assert(std::abs(*stats.lastRttMs - 40.0) < 0.01);
  assert(std::abs(*stats.rttMs - 40.0) < 0.01);
  assert(!stats.jitterMs);

  answerNextPing(manager, socket, 2, 0.060f);
  assert(std::abs(*stats.lastRttMs - 60.0) < 0.01);
  assert(std::abs(*stats.rttMs - 42.5) < 0.01);
  assert(stats.jitterMs && std::abs(*stats.jitterMs - 20.0) < 0.01);

  answerNextPing(manager, socket, 3, 0.050f);
  assert(std::abs(*stats.rttMs - 43.4375) < 0.01);
  assert(std::abs(*stats.jitterMs - 19.375) < 0.01);

  for (std::uint32_t i = 0; i < 60; ++i) answerNextPing(manager, socket, 10 + i, 0.050f);
  assert(std::abs(*stats.rttMs - 50.0) < 0.1);
  assert(*stats.jitterMs < 0.5);
  assert(stats.pongsReceived == 63);
}

static void testLossIsEstimatedFromUnansweredPings() {
  using client::RejectReason;
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();
  manager.openLobby("127.0.0.1", 4242);
  manager.update(0.0f);
  const std::uint64_t firstPing = pingTimestamp(lastSent(socket, network::PacketType::Ping));

  std::uint32_t sequence = 1;
  for (int i = 1; i <= 40; ++i) {
    socket.sent.clear();
    manager.update(0.5f);
    const auto ping = lastSent(socket, network::PacketType::Ping);
    assert(!ping.empty());
    if (i % 4 != 0) {
      socket.enqueue(pongPacket(sequence++, pingTimestamp(ping)), server);
      manager.update(0.0f);
    }
    if (i == 4) {
      assert(stats.pingsLost == 1);
      assert(!stats.lossPercent);
    }
  }
  manager.update(1.0f);

  assert(stats.pingsSent == 42);
  assert(stats.pongsReceived == 30);
  assert(stats.pingsLost == 11);
  assert(stats.lossPercent && std::abs(*stats.lossPercent - 27.5) < 0.01);

  socket.enqueue(pongPacket(sequence++, firstPing), server);
  manager.update(0.0f);
  assert(stats.pongsReceived == 30);
  assert(stats.rejectedFor(RejectReason::Stale) == 1);

  for (int i = 0; i < 40; ++i) {
    socket.sent.clear();
    manager.update(0.5f);
    socket.enqueue(pongPacket(sequence++, pingTimestamp(lastSent(socket, network::PacketType::Ping))), server);
    manager.update(0.0f);
  }
  assert(stats.lossPercent && std::abs(*stats.lossPercent - 2.5) < 0.01);
  assert(stats.pingsLost == 12);
}

static void testRetransmissionsAreCounted() {
  ClientMockSocket socket;
  engine::Registry registry;
  client::ClientNetworkManager manager(socket, registry);
  const network::Endpoint server{"127.0.0.1", 4242};
  const client::NetStats& stats = manager.getStats();

  manager.openLobby("127.0.0.1", 4242);
  manager.requestRooms();
  manager.createRoom(2, 1);
  assert(stats.retransmissionsSent == 0);
  manager.update(0.5f);
  assert(stats.retransmissionsSent == 2);
  socket.enqueue(roomResultPacket(network::PacketType::RoomCreateAck, 1, 5, 0), server);
  manager.update(0.5f);
  assert(stats.retransmissionsSent == 4);

  manager.joinRoom("127.0.0.1", 4242, 5);
  manager.update(0.5f);
  manager.update(0.5f);
  assert(stats.retransmissionsSent == 6);
  assert(countSent(socket, network::PacketType::RoomJoinReq) == 3);

  socket.enqueue(roomResultPacket(network::PacketType::RoomJoinAck, 2, 5, 0), server);
  socket.enqueue(connectAckPacket(3, 0x0102030405060708ull), server);
  manager.update(0.0f);
  assert(manager.isConnected());
  manager.update(0.5f);
  assert(stats.retransmissionsSent == 6);

  assert(stats.retransmissionsReceived == 0);
  socket.enqueue(spawnEntityPacket(10, 2000), server);
  socket.enqueue(spawnEntityPacket(10, 2000), server);
  socket.enqueue(spawnEntityPacket(10, 2000), server);
  socket.enqueue(spawnEntityPacket(11, 2000), server);
  manager.update(0.0f);
  assert(stats.retransmissionsReceived == 2);
  assert(stats.rejectedFor(client::RejectReason::Duplicate) == 3);
  assert(countSent(socket, network::PacketType::Ack) >= 4 + 3);
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
  testPredictionMatchesServerAndReplaysPendingInputs();
  testPredictionCorrectionIsSmoothedOrSnapped();
  testPredictionHandlesSequenceWrapAndStop();
  testDeltaNeedsAKnownBase();
  testEarlyDeltaWaitsForItsBase();
  testLostBaseRequestsFullSnapshot();
  testReliableStateUpdatesIgnoreReordering();
  testSessionStatesFollowTheConnection();
  testConnectRejectEndsTheSession();
  testConnectTimesOutAfterEveryRetry();
  testLobbyOpensTheSocketWithoutConnecting();
  testRoomListIsRequestedAndRead();
  testRoomCreationResults();
  testJoinRoomConnectsToTheGame();
  testJoinRoomFailuresReturnToTheLobby();
  testLeaveRoomCleansTheSession();
  testLeaveRoomTimesOutAndDisconnectCleans();
  testNetStatsStartEmpty();
  testNetStatsCountPacketsAndBytes();
  testNetStatsCountRejectedPacketsByReason();
  testNetStatsMeasureThroughputInBytesPerSecond();
  testPingsMeasureRoundTripTime();
  testRoundTripTimeIsSmoothedWithJitter();
  testLossIsEstimatedFromUnansweredPings();
  testRetransmissionsAreCounted();
  return 0;
}
