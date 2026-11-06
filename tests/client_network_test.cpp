#include "network/ClientNetworkManager.hpp"
#include <cassert>
#include <cstdint>
#include <deque>
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
  deltaPayload << std::uint16_t{1}
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
  testReliableStateUpdatesIgnoreReordering();
  return 0;
}
