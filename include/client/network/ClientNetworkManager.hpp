#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "exception.hpp"
#include "network/INetworkSocket.hpp"
#include "network/PacketStream.hpp"
#include "network/Protocol.hpp"
#include "network/ProtocolExtension.hpp"
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

// Changes compared to the original version (existing packets unchanged):
//  - no rendering: the network layer creates the entity (Transform, Velocity,
//    NetworkId) and notifies the game through callbacks, which decide its look.
//  - position smoothing moved to engine::InterpolationSystem; this class only
//    writes TransformComponent::targetPosition.
//  - robustness: a malformed packet is dropped instead of crashing the client
//    (PacketStreamReader throws on overflow), packets that do not come from the
//    server port are ignored, and the sequence filter only trusts fully parsed
//    packets (a forged huge sequence number can no longer freeze the game).
//  - typed entities (SpawnEntity), health (HealthUpdate), see ProtocolExtension.hpp,
//    and a "shoot" byte at the end of PlayerInput (ignored by servers that do not read it).

namespace rtype::client {
  class ClientNetworkManager {
  public:
    using SpawnCallback =
      std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType, bool isLocal)>;
    using DestroyCallback = std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType)>;
    /// (entity, netId, type, newHealth, maxHealth, previousHealth)
    using HealthCallback =
      std::function<void(engine::Entity, network::NetworkId, network::ext::EntityType, int, int, int)>;

    ClientNetworkManager(network::INetworkSocket& socket, engine::Registry& registry)
      : _socket(socket), _registry(registry) {}

    void setOnEntitySpawned(SpawnCallback callback) { _onSpawn = std::move(callback); }
    /// Called right before the entity is destroyed (player left, enemy killed...).
    void setOnEntityDestroyed(DestroyCallback callback) { _onDestroy = std::move(callback); }
    /// Called after the entity's HealthComponent was updated from a HealthUpdate.
    void setOnHealthChanged(HealthCallback callback) { _onHealth = std::move(callback); }

    void connectToServer(const std::string& host, unsigned short port) {
      _serverEndpoint = {host, port};
      _socket.bind(0);
      _socket.setNonBlocking(true);

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::ConnectReq, _outgoingSeq++, 0};
      network::PacketStreamWriter writer;
      writer << header;
      _socket.send(writer.getBuffer(), _serverEndpoint);
      _isConnected = false;
    }

    void update([[maybe_unused]] float dt) {
      processIncomingPackets();
    }

    void sendInputs(bool up, bool down, bool left, bool right, bool shoot) {
      if (!_isConnected) return;

      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::PlayerInput, _outgoingSeq++, 0
      };

      network::PacketStreamWriter payload;
      payload << _localNetId << up << down << left << right << shoot;

      header.payloadSize = static_cast<std::uint16_t>(payload.getBuffer().size());

      network::PacketStreamWriter finalPacket;
      finalPacket << header;

      network::PacketData packet = finalPacket.getBuffer();
      packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());

      _socket.send(packet, _serverEndpoint);
    }

    void disconnect() {
      if (!_isConnected) return;

      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::Disconnect, _outgoingSeq++, sizeof(_localNetId)
      };
      network::PacketStreamWriter writer;
      writer << header << _localNetId;
      _socket.send(writer.getBuffer(), _serverEndpoint);
      _isConnected = false;
    }

    [[nodiscard]] bool isConnected() const { return _isConnected; }
    [[nodiscard]] network::NetworkId getLocalNetId() const { return _localNetId; }
    [[nodiscard]] std::size_t getDroppedPacketCount() const { return _droppedPackets; }

  private:
    struct RemoteEntity {
      engine::Entity entity;
      network::ext::EntityType type;
    };

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
          ++_droppedPackets;  // truncated or malformed: drop it, keep running
        }
      }
    }

    void handlePacket(const network::PacketData& data) {
      if (data.size() < sizeof(network::PacketHeader)) return;

      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;

      if (header.magic != network::PROTOCOL_MAGIC) return;
      const bool isSync = header.type == network::PacketType::GameStateSync;
      if (isSync && header.sequence <= _lastSyncSeq) return;  // late or duplicated snapshot

      // Extension packet, outside the PacketType enum until the team merges it.
      if (header.type == network::ext::SPAWN_ENTITY) {
        network::NetworkId netId;
        network::ext::EntityType type;
        float x, y;
        reader >> netId >> type >> x >> y;
        spawnEntity(netId, type, x, y);
        return;
      }
      if (header.type == network::ext::HEALTH_UPDATE) {
        network::NetworkId netId;
        std::uint8_t health, maxHealth;
        reader >> netId >> health >> maxHealth;
        updateHealth(netId, health, maxHealth);
        return;
      }

      switch (header.type) {
        case network::PacketType::ConnectAck: {
          reader >> _localNetId;
          _isConnected = true;
          break;
        }
        case network::PacketType::SpawnPlayer: {
          network::NetworkId netId;
          float x, y;
          reader >> netId >> x >> y;
          spawnEntity(netId, network::ext::ENTITY_PLAYER, x, y);
          break;
        }
        case network::PacketType::DestroyEntity: {
          network::NetworkId netId;
          reader >> netId;
          destroyEntity(netId);
          break;
        }
        case network::PacketType::GameStateSync: {
          std::uint16_t count;
          reader >> count;
          // Parse everything first: a truncated snapshot is rejected as a whole.
          _snapshot.clear();
          for (std::uint16_t i = 0; i < count; ++i) {
            network::EntityStateData state{};
            reader >> state;
            _snapshot.push_back(state);
          }
          for (const auto& state : _snapshot) updateEntityState(state);
          _lastSyncSeq = header.sequence;
          break;
        }
        default: break;
      }
    }

    void spawnEntity(network::NetworkId netId, network::ext::EntityType type, float x, float y) {
      if (_networkToEcs.find(netId) != _networkToEcs.end()) return;

      auto entity = _registry.createEntity();
      bool isLocal = type == network::ext::ENTITY_PLAYER && netId == _localNetId;
      _registry.addComponent<engine::TransformComponent>(entity, {{x, y}, 0.0f, {1.0f, 1.0f}, {x, y}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{0.0f, 0.0f}});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {netId, isLocal});

      _networkToEcs[netId] = RemoteEntity{entity, type};
      if (_onSpawn) _onSpawn(entity, netId, type, isLocal);
    }

    void destroyEntity(network::NetworkId netId) {
      auto it = _networkToEcs.find(netId);
      if (it != _networkToEcs.end()) {
        if (_onDestroy) _onDestroy(it->second.entity, netId, it->second.type);
        _registry.destroyEntity(it->second.entity);
        _networkToEcs.erase(it);
      }
    }

    void updateHealth(network::NetworkId netId, int health, int maxHealth) {
      auto it = _networkToEcs.find(netId);
      if (it == _networkToEcs.end()) return;

      const engine::Entity entity = it->second.entity;
      auto* component = _registry.getComponent<engine::HealthComponent>(entity);
      const int previous = component ? component->current : maxHealth;
      _registry.addComponent(entity, engine::HealthComponent{health, maxHealth});
      if (_onHealth) _onHealth(entity, netId, it->second.type, health, maxHealth, previous);
    }

    void updateEntityState(const network::EntityStateData& state) {
      auto it = _networkToEcs.find(state.netId);
      if (it == _networkToEcs.end()) {
        // Unknown entity in a snapshot: the spawn packet was lost. Players are the
        // only entities the original protocol announces, so assume a player.
        spawnEntity(state.netId, network::ext::ENTITY_PLAYER, state.posX, state.posY);
        return;
      }

      auto* transform = _registry.getComponent<engine::TransformComponent>(it->second.entity);
      if (transform) transform->targetPosition = {state.posX, state.posY};
    }

    network::INetworkSocket& _socket;
    engine::Registry& _registry;
    SpawnCallback _onSpawn;
    DestroyCallback _onDestroy;
    HealthCallback _onHealth;
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, RemoteEntity> _networkToEcs;
    std::vector<network::EntityStateData> _snapshot;

    network::NetworkId _localNetId{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastSyncSeq{0};
    std::size_t _droppedPackets{0};
    bool _isConnected{false};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
