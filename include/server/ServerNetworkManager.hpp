#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include "exception.hpp"
#include "GameConstants.hpp"
#include "EnemyManager.hpp"
#include "MissileManager.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace rtype::server {
  struct ConnectedClient {
    network::NetworkId id;
    network::Endpoint endpoint;
    float posX{200.0f};
    float posY{300.0f};
    float velX{0.0f};
    float velY{0.0f};
    float shootCooldown{0.0f};
    std::uint32_t score{0};
    std::uint8_t lives{game::PLAYER_START_LIVES};
    float invincibilityTimer{0.0f};

    bool isAlive() const { return lives > 0; }
    bool isInvincible() const { return invincibilityTimer > 0.0f; }
  };

  class ServerNetworkManager {
  public:
    explicit ServerNetworkManager(network::INetworkSocket& socket) : _socket(socket) {}

    void update(float dt) {
      processIncomingPackets();
      updatePhysics(dt);
      updateMissiles(dt);
      if (!_clients.empty()) _enemyManager.update(dt);
      resolveMissileHits();
      resolveEnemyContacts();
      broadcastGameState();
    }

  private:
    static constexpr float SPAWN_X = 200.0f;
    static constexpr float SPAWN_FIRST_Y = 250.0f;
    static constexpr float SPAWN_SPACING_Y = 150.0f;
    static constexpr network::NetworkId SPAWN_SLOT_COUNT = 4;

    static float getSpawnY(network::NetworkId netId) {
      const network::NetworkId slot = (netId - 1) % SPAWN_SLOT_COUNT;
      return SPAWN_FIRST_Y + static_cast<float>(slot) * SPAWN_SPACING_Y;
    }

    network::PacketData buildPacket(network::PacketType type, const network::PacketStreamWriter& payload) {
      network::PacketHeader header{
        network::PROTOCOL_MAGIC, type, _outgoingSeq++,
        static_cast<std::uint16_t>(payload.getBuffer().size())
      };

      network::PacketStreamWriter writer;
      writer << header;

      network::PacketData packet = writer.getBuffer();
      packet.insert(packet.end(), payload.getBuffer().begin(), payload.getBuffer().end());
      return packet;
    }

    network::PacketData buildSpawnMissilePacket(const Missile& missile) {
      network::MissileSpawnData data{
        missile.id, missile.ownerId,
        missile.posX, missile.posY, missile.velX, missile.velY
      };

      network::PacketStreamWriter payload;
      payload << data;
      return buildPacket(network::PacketType::SpawnMissile, payload);
    }

    void broadcast(const network::PacketData& packet) {
      for (const auto& [id, client] : _clients)
        _socket.send(packet, client.endpoint);
    }

    void processIncomingPackets() {
      network::PacketData data;
      network::Endpoint sender;

      while (_socket.receive(data, sender)) {
        try {
          handlePacket(data, sender);
        } catch (const RtypeError&) {
        }
      }
    }

    void handlePacket(const network::PacketData& data, const network::Endpoint& sender) {
      if (data.size() < sizeof(network::PacketHeader)) return;

      network::PacketStreamReader reader(data);
      network::PacketHeader header{};
      reader >> header;

      if (header.magic != network::PROTOCOL_MAGIC) return;

      switch (header.type) {
        case network::PacketType::ConnectReq: handleConnect(sender); break;
        case network::PacketType::PlayerInput: handleInput(reader); break;
        default: break;
      }
    }

    void handleConnect(const network::Endpoint& sender) {
      network::NetworkId netId = _nextNetId++;
      ConnectedClient client{netId, sender, SPAWN_X, getSpawnY(netId), 0.0f, 0.0f, 0.0f};
      _clients[netId] = client;

      {
        network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::ConnectAck, _outgoingSeq++, sizeof(netId)};
        network::PacketStreamWriter writer;
        writer << header << netId;
        _socket.send(writer.getBuffer(), sender);
      }

      for (const auto& [id, existingClient] : _clients) {
        network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::SpawnPlayer, _outgoingSeq++, 0};
        network::PacketStreamWriter writer;
        writer << header << existingClient.id << existingClient.posX << existingClient.posY;
        _socket.send(writer.getBuffer(), sender);
      }

      for (const auto& [id, existingClient] : _clients) {
        if (id == netId) continue;
        network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::SpawnPlayer, _outgoingSeq++, 0};
        network::PacketStreamWriter writer;
        writer << header << client.id << client.posX << client.posY;
        _socket.send(writer.getBuffer(), existingClient.endpoint);
      }

      for (const auto& [id, missile] : _missileManager.getMissiles())
        _socket.send(buildSpawnMissilePacket(missile), sender);
    }

    void handleInput(network::PacketStreamReader& reader) {
      network::NetworkId netId;
      bool up = false, down = false, left = false, right = false, shoot = false;
      reader >> netId >> up >> down >> left >> right >> shoot;

      auto it = _clients.find(netId);
      if (it == _clients.end() || !it->second.isAlive()) return;

      const float speed = game::PLAYER_SPEED;
      it->second.velX = 0.0f;
      it->second.velY = 0.0f;
      if (up) it->second.velY -= speed;
      if (down) it->second.velY += speed;
      if (left) it->second.velX -= speed;
      if (right) it->second.velX += speed;

      if (shoot && it->second.shootCooldown <= 0.0f) {
        it->second.shootCooldown = game::PLAYER_SHOOT_COOLDOWN;
        spawnMissileFor(it->second);
      }
    }

    void spawnMissileFor(const ConnectedClient& client) {
      const Missile missile = _missileManager.spawn(
        client.id,
        client.posX + game::PLAYER_MUZZLE_OFFSET_X,
        client.posY + game::PLAYER_MUZZLE_OFFSET_Y);
      broadcast(buildSpawnMissilePacket(missile));
    }

    void updatePhysics(float dt) {
      for (auto& [id, client] : _clients) {
        client.posX = std::clamp(client.posX + client.velX * dt, 0.0f, game::WORLD_WIDTH - game::PLAYER_WIDTH);
        client.posY = std::clamp(client.posY + client.velY * dt, 0.0f, game::WORLD_HEIGHT - game::PLAYER_HEIGHT);
        if (client.shootCooldown > 0.0f) client.shootCooldown -= dt;
        if (client.invincibilityTimer > 0.0f) client.invincibilityTimer -= dt;
      }
    }

    void resolveMissileHits() {
      std::vector<network::NetworkId> spentMissileIds;

      for (const auto& [missileId, missile] : _missileManager.getMissiles()) {
        const game::Hitbox missileHitbox = MissileManager::getHitbox(missile);

        for (const auto& [enemyId, enemy] : _enemyManager.getEnemies()) {
          if (!missileHitbox.intersects(EnemyManager::getHitbox(enemy))) continue;

          spentMissileIds.push_back(missileId);
          const Enemy target = enemy;
          if (_enemyManager.applyDamage(enemyId, game::MISSILE_DAMAGE)) onEnemyKilled(target, missile.ownerId);
          break;
        }
      }

      for (network::NetworkId missileId : spentMissileIds) {
        _missileManager.remove(missileId);
        broadcastDestroyEntity(missileId);
      }
    }

    void onEnemyKilled(const Enemy& enemy, network::NetworkId killerId) {
      auto killer = _clients.find(killerId);
      if (killer != _clients.end()) killer->second.score += EnemyManager::getScoreValue(enemy.type);

      broadcastExplosion(EnemyManager::getHitbox(enemy), EnemyManager::getExplosionScale(enemy.type));
    }

    void resolveEnemyContacts() {
      for (auto& [clientId, client] : _clients) {
        if (!client.isAlive() || client.isInvincible()) continue;
        const game::Hitbox playerHitbox = game::PLAYER_HITBOX.movedTo(client.posX, client.posY);

        for (const auto& [enemyId, enemy] : _enemyManager.getEnemies()) {
          const game::Hitbox enemyHitbox = EnemyManager::getHitbox(enemy);
          if (!playerHitbox.intersects(enemyHitbox)) continue;

          broadcastExplosion(enemyHitbox, EnemyManager::getExplosionScale(enemy.type));
          _enemyManager.remove(enemyId);
          damagePlayer(client);
          break;
        }
      }
    }

    void damagePlayer(ConnectedClient& client) {
      --client.lives;
      client.invincibilityTimer = game::PLAYER_INVINCIBILITY_DURATION;

      if (client.isAlive()) return;

      client.velX = 0.0f;
      client.velY = 0.0f;
      broadcastExplosion(game::PLAYER_HITBOX.movedTo(client.posX, client.posY), game::PLAYER_EXPLOSION_SCALE);
    }

    void broadcastExplosion(const game::Hitbox& area, float scale) {
      network::ExplosionData data{area.x + area.width / 2.0f, area.y + area.height / 2.0f, scale};

      network::PacketStreamWriter payload;
      payload << data;
      broadcast(buildPacket(network::PacketType::SpawnExplosion, payload));
    }

    void broadcastDestroyEntity(network::NetworkId netId) {
      network::PacketStreamWriter payload;
      payload << netId;
      broadcast(buildPacket(network::PacketType::DestroyEntity, payload));
    }

    void updateMissiles(float dt) {
      for (network::NetworkId id : _missileManager.update(dt))
        broadcastDestroyEntity(id);
    }

    void broadcastGameState() {
      if (_clients.empty()) return;

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::GameStateSync, _outgoingSeq++, 0};
      network::PacketStreamWriter writer;
      writer << header;

      auto count = static_cast<std::uint16_t>(_clients.size());
      writer << count;

      for (const auto& [id, client] : _clients) {
        network::EntityStateData state{
          client.id, client.posX, client.posY, client.velX, client.velY,
          client.score, client.lives, static_cast<std::uint8_t>(client.isInvincible())
        };
        writer << state;
      }

      const auto& enemies = _enemyManager.getEnemies();
      writer << static_cast<std::uint16_t>(enemies.size());

      for (const auto& [id, enemy] : enemies) {
        network::EnemyStateData state{enemy.id, static_cast<std::uint8_t>(enemy.type), enemy.posX, enemy.posY};
        writer << state;
      }

      for (const auto& [id, client] : _clients)
        _socket.send(writer.getBuffer(), client.endpoint);
    }

    network::INetworkSocket& _socket;
    MissileManager _missileManager;
    EnemyManager _enemyManager;
    std::unordered_map<network::NetworkId, ConnectedClient> _clients;
    network::NetworkId _nextNetId{1};
    std::uint32_t _outgoingSeq{1};
  };
}

#endif // !SERVERNETWORKMANAGER_HPP
