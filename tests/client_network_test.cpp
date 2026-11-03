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
  assert(socket.sent.empty());

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
  return 0;
}
