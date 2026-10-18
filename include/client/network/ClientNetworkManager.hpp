#ifndef CLIENTNETWORKMANAGER_HPP
#define CLIENTNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "exception.hpp"
#include "game/IEnemySpriteFactory.hpp"
#include "game/IExplosionSpriteFactory.hpp"
#include "game/IMissileSpriteFactory.hpp"
#include "game/IPlayerSpriteFactory.hpp"
#include <unordered_map>
#include <unordered_set>

namespace rtype::client {
  class ClientNetworkManager {
  public:
    ClientNetworkManager(network::INetworkSocket& socket,
                         engine::Registry& registry,
                         IPlayerSpriteFactory& playerSpriteFactory,
                         IMissileSpriteFactory& missileSpriteFactory,
                         IEnemySpriteFactory& enemySpriteFactory,
                         IExplosionSpriteFactory& explosionSpriteFactory)
      : _socket(socket),
        _registry(registry),
        _playerSpriteFactory(playerSpriteFactory),
        _missileSpriteFactory(missileSpriteFactory),
        _enemySpriteFactory(enemySpriteFactory),
        _explosionSpriteFactory(explosionSpriteFactory) {}

    void connectToServer(const std::string& host, unsigned short port) {
      _serverEndpoint = {host, port};
      _socket.bind(0);
      _socket.setNonBlocking(true);
      _isConnected = false;
      sendConnectRequest();
    }

    void disconnect() {
      if (_hasDisconnected) return;
      _hasDisconnected = true;
      if (!_isConnected) return;

      network::PacketStreamWriter payload;
      payload << _localNetId;
      sendPacket(network::PacketType::Disconnect, payload);
      _isConnected = false;
    }

    void update(float dt) {
      processIncomingPackets();
      forgetDestroyedEntities();
      if (!_isConnected) {
        if (!_hasDisconnected) retryConnectRequest(dt);
        return;
      }

      _registry.view<engine::TransformComponent>([this, dt](engine::Entity entity, engine::TransformComponent& transform) {
        if (_registry.getComponent<engine::ProjectileComponent>(entity)) return;

        float lerpFactor = 15.0f * dt;
        if (lerpFactor > 1.0f) lerpFactor = 1.0f;
        transform.position.x += (transform.targetPosition.x - transform.position.x) * lerpFactor;
        transform.position.y += (transform.targetPosition.y - transform.position.y) * lerpFactor;
      });
    }

    void sendInputs(bool up, bool down, bool left, bool right, bool shoot) {
      if (!_isConnected) return;

      network::PacketStreamWriter payload;
      payload << _localNetId << up << down << left << right << shoot;
      sendPacket(network::PacketType::PlayerInput, payload);
    }

    [[nodiscard]] bool isConnected() const { return _isConnected; }
    [[nodiscard]] network::NetworkId getLocalNetId() const { return _localNetId; }

  private:
    static constexpr float CONNECT_RETRY_DELAY = 0.5f;

    void sendPacket(network::PacketType type, const network::PacketStreamWriter& payload) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++,
        static_cast<std::uint16_t>(payload.getBuffer().size())
      };

      network::PacketStreamWriter writer;
      writer << header;

      network::PacketData packet = writer.getBuffer();
      packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());
      _socket.send(packet, _serverEndpoint);
    }

    void sendConnectRequest() {
      sendPacket(network::PacketType::ConnectReq, network::PacketStreamWriter{});
      _connectRetryTimer = CONNECT_RETRY_DELAY;
    }

    void retryConnectRequest(float dt) {
      _connectRetryTimer -= dt;
      if (_connectRetryTimer <= 0.0f) sendConnectRequest();
    }

    void processIncomingPackets() {
      network::PacketData data;
      network::Endpoint sender;

      while (_socket.receive(data, sender)) {
        try {
          handlePacket(data);
        } catch (const RtypeError&) {
        }
      }
    }

    void handlePacket(const network::PacketData& data) {
      if (data.size() < sizeof(network::PacketHeader)) return;

      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;

      if (header.magic != network::PROTOCOL_MAGIC) return;
      if (header.sequence <= _lastIncomingSeq && header.type == network::PacketType::GameStateSync) return;

      _lastIncomingSeq = header.sequence;

      switch (header.type) {
        case network::PacketType::ConnectAck: {
          if (_hasDisconnected) break;
          reader >> _localNetId;
          _isConnected = true;
          break;
        }
        case network::PacketType::SpawnPlayer: {
          network::NetworkId netId;
          float x, y;
          reader >> netId >> x >> y;
          spawnPlayer(netId, x, y);
          break;
        }
        case network::PacketType::SpawnExplosion: {
          network::ExplosionData explosion{};
          reader >> explosion;
          spawnExplosion(explosion);
          break;
        }
        case network::PacketType::SpawnMissile: {
          network::MissileSpawnData missile{};
          reader >> missile;
          spawnMissile(missile);
          break;
        }
        case network::PacketType::DestroyEntity: {
          network::NetworkId netId;
          reader >> netId;
          destroyNetworkEntity(netId);
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
          readEnemyStates(reader);
          break;
        }
        default: break;
      }
    }

    void spawnPlayer(network::NetworkId netId, float x, float y) {
      if (_networkToEcs.find(netId) != _networkToEcs.end()) return;

      auto entity = _registry.createEntity();
      auto sprite = _playerSpriteFactory.createSprite(netId);

      bool isLocal = (netId == _localNetId);
      _registry.addComponent<engine::TransformComponent>(entity, {{x, y}, 0.0f, _playerSpriteFactory.getSpriteScale(), {x, y}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{0.0f, 0.0f}});
      _registry.addComponent<engine::SpriteComponent>(entity, {sprite});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {netId, isLocal});
      _registry.addComponent<engine::PlayerComponent>(entity, {0, game::PLAYER_START_LIVES, false});
      _registry.addComponent<engine::ShipTiltComponent>(entity, _playerSpriteFactory.createTilt(netId));

      _networkToEcs[netId] = entity;
    }

    void spawnMissile(const network::MissileSpawnData& missile) {
      if (_networkToEcs.find(missile.missileId) != _networkToEcs.end()) return;

      auto entity = _registry.createEntity();
      auto sprite = _missileSpriteFactory.createSprite();

      _registry.addComponent<engine::TransformComponent>(entity, {{missile.posX, missile.posY}, 0.0f, _missileSpriteFactory.getSpriteScale(), {missile.posX, missile.posY}});
      _registry.addComponent<engine::VelocityComponent>(entity, {{missile.velX, missile.velY}});
      _registry.addComponent<engine::SpriteComponent>(entity, {sprite});
      _registry.addComponent<engine::ProjectileComponent>(entity, {missile.ownerId});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {missile.missileId, false});

      _networkToEcs[missile.missileId] = entity;
    }

    void readEnemyStates(network::PacketStreamReader& reader) {
      std::uint16_t count;
      reader >> count;

      std::unordered_set<network::NetworkId> receivedIds;
      for (std::uint16_t i = 0; i < count; ++i) {
        network::EnemyStateData state{};
        reader >> state;
        updateEnemyState(state);
        receivedIds.insert(state.enemyId);
      }

      std::erase_if(_enemyIds, [&](network::NetworkId enemyId) {
        if (receivedIds.contains(enemyId)) return false;
        destroyNetworkEntity(enemyId);
        return true;
      });
    }

    void updateEnemyState(const network::EnemyStateData& state) {
      auto it = _networkToEcs.find(state.enemyId);
      if (it == _networkToEcs.end()) {
        spawnEnemy(state);
        return;
      }

      auto* transform = _registry.getComponent<engine::TransformComponent>(it->second);
      if (transform) transform->targetPosition = {state.posX, state.posY};
    }

    void spawnEnemy(const network::EnemyStateData& state) {
      if (state.enemyType >= static_cast<std::uint8_t>(game::EnemyType::Count)) return;

      const auto type = static_cast<game::EnemyType>(state.enemyType);
      auto entity = _registry.createEntity();
      auto sprite = _enemySpriteFactory.createSprite(type);

      _registry.addComponent<engine::TransformComponent>(entity, {{state.posX, state.posY}, 0.0f, _enemySpriteFactory.getSpriteScale(), {state.posX, state.posY}});
      _registry.addComponent<engine::SpriteComponent>(entity, {sprite});
      _registry.addComponent<engine::AnimationComponent>(entity, _enemySpriteFactory.createAnimation(type));
      _registry.addComponent<engine::EnemyComponent>(entity, {state.enemyType});
      _registry.addComponent<engine::NetworkIdComponent>(entity, {state.enemyId, false});

      _networkToEcs[state.enemyId] = entity;
      _enemyIds.insert(state.enemyId);
    }

    void forgetDestroyedEntities() {
      std::erase_if(_networkToEcs, [this](const auto& entry) {
        return !_registry.getComponent<engine::NetworkIdComponent>(entry.second);
      });
    }

    void destroyNetworkEntity(network::NetworkId netId) {
      auto it = _networkToEcs.find(netId);
      if (it != _networkToEcs.end()) {
        _registry.destroyEntity(it->second);
        _networkToEcs.erase(it);
      }
    }

    void updateEntityState(const network::EntityStateData& state) {
      if (_networkToEcs.find(state.netId) == _networkToEcs.end())
        spawnPlayer(state.netId, state.posX, state.posY);

      const engine::Entity entity = _networkToEcs.at(state.netId);

      auto* transform = _registry.getComponent<engine::TransformComponent>(entity);
      if (transform) transform->targetPosition = {state.posX, state.posY};

      auto* velocity = _registry.getComponent<engine::VelocityComponent>(entity);
      if (velocity) velocity->velocity = {state.velX, state.velY};

      auto* player = _registry.getComponent<engine::PlayerComponent>(entity);
      if (player) *player = {state.score, state.lives, state.isInvincible != 0};
    }

    void spawnExplosion(const network::ExplosionData& explosion) {
      if (!(explosion.scale > 0.0f && explosion.scale <= MAX_EXPLOSION_SCALE)) return;

      const engine::Vector2f frameSize = _explosionSpriteFactory.getFrameSize();
      const engine::Vector2f position{
        explosion.centerX - frameSize.x * explosion.scale / 2.0f,
        explosion.centerY - frameSize.y * explosion.scale / 2.0f
      };

      auto entity = _registry.createEntity();
      _registry.addComponent<engine::TransformComponent>(entity, {position, 0.0f, {explosion.scale, explosion.scale}, position});
      _registry.addComponent<engine::SpriteComponent>(entity, {_explosionSpriteFactory.createSprite()});
      _registry.addComponent<engine::AnimationComponent>(entity, _explosionSpriteFactory.createAnimation());
      _registry.addComponent<engine::LifetimeComponent>(entity, {_explosionSpriteFactory.getDuration()});
    }

    static constexpr float MAX_EXPLOSION_SCALE = 16.0f;

    network::INetworkSocket& _socket;
    engine::Registry& _registry;
    IPlayerSpriteFactory& _playerSpriteFactory;
    IMissileSpriteFactory& _missileSpriteFactory;
    IEnemySpriteFactory& _enemySpriteFactory;
    IExplosionSpriteFactory& _explosionSpriteFactory;
    network::Endpoint _serverEndpoint;
    std::unordered_map<network::NetworkId, engine::Entity> _networkToEcs;
    std::unordered_set<network::NetworkId> _enemyIds;

    network::NetworkId _localNetId{0};
    std::uint32_t _outgoingSeq{1};
    std::uint32_t _lastIncomingSeq{0};
    bool _isConnected{false};
    float _connectRetryTimer{0.0f};
    bool _hasDisconnected{false};
  };
}

#endif // !CLIENTNETWORKMANAGER_HPP
