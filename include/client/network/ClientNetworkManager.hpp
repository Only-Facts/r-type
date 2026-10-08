#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "gfx/SFMLSprite.hpp"
#include "gfx/ITextureManager.hpp"
#include <unordered_map>

namespace rtype::client {
  class ClientNetworkManager {
  public:
    ClientNetworkManager(network::INetworkSocket& socket,
                         engine::Registry& registry,
                         engine::ITextureManager& texMgr)
      : _socket(socket), _registry(registry), _texMgr(texMgr) {}

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

    void update(float dt) {
      processIncomingPackets();
      if (!_isConnected) return;

      _registry.view<engine::TransformComponent>([dt](engine::Entity, engine::TransformComponent& transform) {
        float lerpFactor = 15.0f * dt;
        if (lerpFactor > 1.0f) lerpFactor = 1.0f;
        transform.position.x += (transform.targetPosition.x - transform.position.x) * lerpFactor;
        transform.position.y += (transform.targetPosition.y - transform.position.y) * lerpFactor;
      });
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
      auto tex = _texMgr.loadTexture("assets/player.png");
      auto sprite = std::make_shared<engine::SFMLSprite>(tex);

      bool isLocal = (netId == _localNetId);
      _registry.addComponent<engine::TransformComponent>(entity, {{x, y}, 0.0f, {2.0f, 2.0f}, {x, y}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{0.0f, 0.0f}});
      _registry.addComponent<engine::SpriteComponent>(entity, {sprite});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {netId, isLocal});

      _networkToEcs[netId] = entity;
    }

    void destroyRemotePlayer(network::NetworkId netId) {
      auto it = _networkToEcs.find(netId);
      if (it != _networkToEcs.end()) {
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
    engine::ITextureManager& _texMgr;
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, engine::Entity> _networkToEcs;

    network::NetworkId _localNetId{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastIncomingSeq{0};
    bool _isConnected{false};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
