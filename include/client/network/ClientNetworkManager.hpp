#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "network/INetworkSocket.hpp"
#include "network/PacketStream.hpp"
#include "network/Protocol.hpp"
#include <functional>
#include <unordered_map>
#include <utility>

// Changes compared to the previous version (protocol untouched):
//  - no more dependency on the texture manager: the network layer does not render.
//    It creates the entity (Transform, Velocity, NetworkId) and notifies the game
//    through the spawn callback, which decides how the entity looks.
//  - position smoothing moved to engine::InterpolationSystem; this class only
//    writes TransformComponent::targetPosition.
//  - a destroy callback lets the game react when an entity leaves (HUD, sound).

namespace rtype::client {
  class ClientNetworkManager {
  public:
    using SpawnCallback = std::function<void(engine::Entity, network::NetworkId, bool isLocal)>;
    using DestroyCallback = std::function<void(engine::Entity, network::NetworkId)>;

    ClientNetworkManager(network::INetworkSocket& socket, engine::Registry& registry)
      : _socket(socket), _registry(registry) {}

    void setOnEntitySpawned(SpawnCallback callback) { _onSpawn = std::move(callback); }
    /// Called right before the entity is destroyed (player left, crashed, died...).
    void setOnEntityDestroyed(DestroyCallback callback) { _onDestroy = std::move(callback); }

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

    void sendInputs(bool up, bool down, bool left, bool right) {
      if (!_isConnected) return;

      network::PacketHeader header{
        network::PROTOCOL_MAGIC, network::PacketType::PlayerInput, _outgoingSeq++, 0
      };

      network::PacketStreamWriter payload;
      payload << _localNetId << up << down << left << right;

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

  private:
    void processIncomingPackets() {
      network::PacketData data;
      network::Endpoint sender;

      while (_socket.receive(data, sender)) {
        if (data.size() < sizeof(network::PacketHeader)) continue;

        network::PacketStreamReader reader(data);
        network::PacketHeader header{};
        reader >> header;

        if (header.magic != network::PROTOCOL_MAGIC) continue;
        if (header.sequence <= _lastIncomingSeq && header.type == network::PacketType::GameStateSync) continue;

        _lastIncomingSeq = header.sequence;

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
            spawnRemotePlayer(netId, x, y);
            break;
          }
          case network::PacketType::DestroyEntity: {
            network::NetworkId netId;
            reader >> netId;
            destroyRemotePlayer(netId);
            break;
          }
          case network::PacketType::GameStateSync: {
            std::uint16_t count;
            reader >> count;
            for (std::uint16_t i = 0; i < count; ++i) {
              network::EntityStateData state{};
              reader >> state;
              updateEntityState(state);
            }
            break;
          }
          default: break;
        }
      }
    }

    void spawnRemotePlayer(network::NetworkId netId, float x, float y) {
      if (_networkToEcs.find(netId) != _networkToEcs.end()) return;

      auto entity = _registry.createEntity();
      bool isLocal = (netId == _localNetId);
      _registry.addComponent<engine::TransformComponent>(entity, {{x, y}, 0.0f, {1.0f, 1.0f}, {x, y}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{0.0f, 0.0f}});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {netId, isLocal});

      _networkToEcs[netId] = entity;
      if (_onSpawn) _onSpawn(entity, netId, isLocal);
    }

    void destroyRemotePlayer(network::NetworkId netId) {
      auto it = _networkToEcs.find(netId);
      if (it != _networkToEcs.end()) {
        if (_onDestroy) _onDestroy(it->second, netId);
        _registry.destroyEntity(it->second);
        _networkToEcs.erase(it);
      }
    }

    void updateEntityState(const network::EntityStateData& state) {
      auto it = _networkToEcs.find(state.netId);
      if (it == _networkToEcs.end()) {
        spawnRemotePlayer(state.netId, state.posX, state.posY);
        return;
      }

      auto* transform = _registry.getComponent<engine::TransformComponent>(it->second);
      if (transform) transform->targetPosition = {state.posX, state.posY};
    }

    network::INetworkSocket& _socket;
    engine::Registry& _registry;
    SpawnCallback _onSpawn;
    DestroyCallback _onDestroy;
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, engine::Entity> _networkToEcs;

    network::NetworkId _localNetId{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastIncomingSeq{0};
    bool _isConnected{false};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
