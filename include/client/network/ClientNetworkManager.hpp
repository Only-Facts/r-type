#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "exception.hpp"
#include "network/INetworkSocket.hpp"
#include "network/PacketStream.hpp"
#include "network/Protocol.hpp"
#include "network/ProtocolExtension.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
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

    ClientNetworkManager(network::INetworkSocket& socket, engine::Registry& registry)
      : _socket(socket), _registry(registry) {}

    void setOnEntitySpawned(SpawnCallback callback) { _onSpawn = std::move(callback); }

    void setOnEntityDestroyed(DestroyCallback callback) { _onDestroy = std::move(callback); }

    void setOnHealthChanged(HealthCallback callback) { _onHealth = std::move(callback); }
    void setOnAugmentChanged(AugmentCallback callback) { _onAugment = std::move(callback); }

    static constexpr std::uint8_t NO_PREFERENCE = 0xFF;

    void connectToServer(const std::string& host, unsigned short port,
                         std::optional<std::uint8_t> difficulty = std::nullopt,
                         std::optional<std::uint8_t> skin = std::nullopt) {
      _serverEndpoint = {host, port};
      _socket.bind(0);
      _socket.setNonBlocking(true);

      const std::uint16_t payloadSize = skin ? 2 : difficulty ? 1 : 0;
      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::ConnectReq, _outgoingSeq++,
                                   payloadSize};
      network::PacketStreamWriter writer;
      writer << header;
      if (difficulty || skin) writer << difficulty.value_or(NO_PREFERENCE);
      if (skin) writer << *skin;
      _socket.send(writer.getBuffer(), _serverEndpoint);
      _isConnected = false;
      _serverDifficulty.reset();
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

      if (header.magic != network::PROTOCOL_MAGIC) return;
      const bool isSync = header.type == network::PacketType::GameStateSync;
      if (isSync && header.sequence <= _lastSyncSeq) return;

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
      if (header.type == network::ext::AUGMENT_UPDATE) {
        network::NetworkId netId;
        network::ext::AugmentId augment;
        std::uint8_t level;
        float remaining;
        reader >> netId >> augment >> level >> remaining;
        auto it = _networkToEcs.find(netId);
        if (it != _networkToEcs.end() && _onAugment)
          _onAugment(it->second.entity, netId, it->second.type, augment, level, remaining);
        return;
      }

      switch (header.type) {
        case network::PacketType::ConnectAck: {
          reader >> _localNetId;
          if (reader.hasMoreData()) {
            std::uint8_t difficulty = 0;
            reader >> difficulty;
            _serverDifficulty = difficulty;
          }
          _isConnected = true;
          break;
        }
        case network::PacketType::SpawnPlayer: {
          network::NetworkId netId;
          float x, y;
          reader >> netId >> x >> y;
          if (reader.hasMoreData()) {
            std::uint8_t skin = NO_PREFERENCE;
            reader >> skin;
            if (skin != NO_PREFERENCE) _playerSkins[netId] = skin;
          }
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
        _playerSkins.erase(netId);
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
    AugmentCallback _onAugment;
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, RemoteEntity> _networkToEcs;
    std::vector<network::EntityStateData> _snapshot;

    network::NetworkId _localNetId{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastSyncSeq{0};
    std::size_t _droppedPackets{0};
    std::optional<std::uint8_t> _serverDifficulty;
    std::unordered_map<network::NetworkId, std::uint8_t> _playerSkins;
    bool _isConnected{false};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
