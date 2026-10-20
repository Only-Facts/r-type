#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

namespace rtype::server {

  enum class ServerEntityType : std::uint8_t {
    Player = 0,
    Bydos = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
    BonusDouble = 4,
    BonusTriple = 5,
    BonusHeal = 6,
    BonusDamage = 7,
    PlayerMissileHeavy = 8
  };

  struct ServerEntity {
    network::NetworkId id;
    ServerEntityType type;
    float posX{0.f};
    float posY{0.f};
    float velX{0.f};
    float velY{0.f};
    int hp{1};
    int maxHp{1};
    network::NetworkId ownerId{0};
  };

  struct ConnectedClient {
    network::NetworkId id;
    network::Endpoint endpoint;
    float posX{200.0f};
    float posY{300.0f};
    float velX{0.0f};
    float velY{0.0f};
    float idleTime{0.0f};
    std::uint8_t difficulty{1};
    std::uint8_t skin{0xFF};
    int hp{5};
    int maxHp{5};
    bool dead{false};
    float deathTimer{0.f};
    float shootCooldown{0.f};
    bool shootPressed{false};
  };

  class ServerNetworkManager {
  public:
    explicit ServerNetworkManager(network::INetworkSocket& socket) : _socket(socket) {}

    void update(float dt) {
      processIncomingPackets();
      removeTimedOutClients(dt);
      updateGameLogic(dt);
      broadcastGameState();
    }

  private:
    static constexpr float CLIENT_TIMEOUT = 3.0f;

    void processIncomingPackets() {
      network::PacketData data;
      network::Endpoint sender;

      while (_socket.receive(data, sender)) {
        try {
          handlePacket(data, sender);
        } catch (const RtypeError&) {
          continue; 
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
        case network::PacketType::ConnectReq: handleConnect(reader, sender, header.payloadSize); break;
        case network::PacketType::PlayerInput: handleInput(reader, sender); break;
        case network::PacketType::Disconnect: handleDisconnect(reader); break;
        default: break;
      }
    }

    void handleConnect(network::PacketStreamReader& reader, const network::Endpoint& sender, std::uint16_t payloadSize) {
      for (auto& [id, existing] : _clients) {
        if (existing.endpoint.address == sender.address && existing.endpoint.port == sender.port) {
          sendConnectAck(existing);
          resendWorldToClient(existing);
          return;
        }
      }

      std::uint8_t diff = 1;
      std::uint8_t skin = 0xFF;
      network::NetworkId requestedPrevId = 0;

      if (payloadSize >= 1 && reader.hasMoreData()) reader >> diff;
      if (payloadSize >= 2 && reader.hasMoreData()) reader >> skin;
      if (payloadSize >= 6 && reader.hasMoreData()) reader >> requestedPrevId;

      if (!_clients.empty()) {
        diff = _currentDifficulty;
      } else {
        _currentDifficulty = (diff == 0xFF ? 1 : diff);
      }

      network::NetworkId netId = 0;
      if (requestedPrevId >= 1000 && requestedPrevId < 2000 && _clients.find(requestedPrevId) == _clients.end()) {
        netId = requestedPrevId;
      } else {
        netId = _nextPlayerId++;
        if (_nextPlayerId >= 2000) _nextPlayerId = 1000;
      }

      int startHp = 5;
      if (_currentDifficulty == 0) startHp = 8;
      else if (_currentDifficulty == 2) startHp = 4;

      ConnectedClient client{netId, sender, 200.0f + ((netId % 4) * 50.0f), 300.0f + ((netId % 4) * 40.0f), 0.0f, 0.0f, 0.0f, _currentDifficulty, skin, startHp, startHp, false, 0.f, 0.f, false};
      _clients[netId] = client;

      sendConnectAck(client);

      for (const auto& [id, existing] : _clients) {
        if (id == netId) continue;
        sendSpawnPlayer(existing, sender);
        sendHealthUpdate(existing.id, existing.hp, existing.maxHp, sender);
      }

      for (const auto& [id, existing] : _clients) {
        sendSpawnPlayer(client, existing.endpoint);
        sendHealthUpdate(netId, client.hp, client.maxHp, existing.endpoint);
      }

      for (const auto& [entId, ent] : _entities) {
        sendSpawnEntity(ent, sender);
        if (ent.maxHp > 1) {
          sendHealthUpdate(entId, ent.hp, ent.maxHp, sender);
        }
      }
    }

    void sendConnectAck(const ConnectedClient& client) {
      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::ConnectAck, _outgoingSeq++, 5};
      network::PacketStreamWriter writer;
      writer << header << client.id << client.difficulty;
      _socket.send(writer.getBuffer(), client.endpoint);
    }

    void resendWorldToClient(const ConnectedClient& client) {
      for (const auto& [id, existing] : _clients) {
        if (id == client.id) continue;
        sendSpawnPlayer(existing, client.endpoint);
        sendHealthUpdate(existing.id, existing.hp, existing.maxHp, client.endpoint);
      }
      for (const auto& [entId, ent] : _entities) {
        sendSpawnEntity(ent, client.endpoint);
        if (ent.maxHp > 1) sendHealthUpdate(entId, ent.hp, ent.maxHp, client.endpoint);
      }
    }

    void sendSpawnPlayer(const ConnectedClient& target, const network::Endpoint& recipient) {
      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::SpawnPlayer, _outgoingSeq++, 13};
      network::PacketStreamWriter writer;
      writer << header << target.id << target.posX << target.posY << target.skin;
      _socket.send(writer.getBuffer(), recipient);
    }

    void sendSpawnEntity(const ServerEntity& ent, const network::Endpoint& recipient) {
      network::PacketHeader header{network::PROTOCOL_MAGIC, static_cast<network::PacketType>(0x12), _outgoingSeq++, 13};
      network::PacketStreamWriter writer;
      writer << header << ent.id << static_cast<std::uint8_t>(ent.type) << ent.posX << ent.posY;
      _socket.send(writer.getBuffer(), recipient);
    }

    void sendHealthUpdate(network::NetworkId netId, std::uint8_t hp, std::uint8_t maxHp, const network::Endpoint& recipient) {
      network::PacketHeader header{network::PROTOCOL_MAGIC, static_cast<network::PacketType>(0x13), _outgoingSeq++, 6};
      network::PacketStreamWriter writer;
      writer << header << netId << hp << maxHp;
      _socket.send(writer.getBuffer(), recipient);
    }

    void handleInput(network::PacketStreamReader& reader, const network::Endpoint& sender) {
      network::NetworkId netId;
      bool up, down, left, right;
      reader >> netId >> up >> down >> left >> right;

      bool shoot = false;
      if (reader.hasMoreData()) {
        reader >> shoot;
      }

      auto it = _clients.find(netId);
      if (it != _clients.end()) {
        if (it->second.endpoint.address != sender.address || it->second.endpoint.port != sender.port) {
          return;
        }

        it->second.idleTime = 0.0f;
        it->second.shootPressed = shoot;

        if (it->second.dead) {
          it->second.velX = 0.f;
          it->second.velY = 0.f;
          return;
        }

        float speed = 300.0f;
        it->second.velX = 0.0f;
        it->second.velY = 0.0f;
        if (up) it->second.velY -= speed;
        if (down) it->second.velY += speed;
        if (left) it->second.velX -= speed;
        if (right) it->second.velX += speed;
      }
    }

    void handleDisconnect(network::PacketStreamReader& reader) {
      network::NetworkId netId;
      reader >> netId;
      removeClient(netId);
    }

    void removeClient(network::NetworkId netId) {
      auto it = _clients.find(netId);
      if (it == _clients.end()) return;

      _clients.erase(it);

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::DestroyEntity, _outgoingSeq++, sizeof(netId)};
      network::PacketStreamWriter writer;
      writer << header << netId;
      for (const auto& [id, other] : _clients)
        _socket.send(writer.getBuffer(), other.endpoint);
    }

    void removeTimedOutClients(float dt) {
      std::vector<network::NetworkId> dead;
      for (auto& [id, client] : _clients) {
        client.idleTime += dt;
        if (client.idleTime > CLIENT_TIMEOUT) dead.push_back(id);
      }
      for (auto id : dead) removeClient(id);
    }

    void updateGameLogic(float dt) {
      for (auto& [id, client] : _clients) {
        if (client.dead) {
          client.deathTimer -= dt;
          if (client.deathTimer <= 0.f) {
            removeClient(id);
          }
          continue;
        }

        client.posX += client.velX * dt;
        client.posY += client.velY * dt;

        if (client.shootCooldown > 0.f) {
          client.shootCooldown -= dt;
        }
        if (client.shootPressed && client.shootCooldown <= 0.f) {
          client.shootCooldown = 0.25f;
          spawnPlayerMissile(client);
        }
      }

      _bydoSpawnTimer += dt;
      float spawnInterval = 1.2f;
      if (_currentDifficulty == 0) spawnInterval = 1.6f;
      else if (_currentDifficulty == 2) spawnInterval = 0.8f;

      if (_bydoSpawnTimer >= spawnInterval) {
        _bydoSpawnTimer = 0.f;
        spawnBydo();
      }

      std::vector<network::NetworkId> destroyedEntities;
      for (auto& [entId, ent] : _entities) {
        ent.posX += ent.velX * dt;
        ent.posY += ent.velY * dt;

        if (ent.posX < -100.f || ent.posX > 2120.f || ent.posY < -100.f || ent.posY > 1180.f) {
          destroyedEntities.push_back(entId);
        }
      }

      float enemyShootInterval = (_currentDifficulty == 0 ? 4.0f : (_currentDifficulty == 1 ? 2.0f : 1.11f));
      _enemyShootTimer += dt;
      if (_enemyShootTimer >= enemyShootInterval) {
        _enemyShootTimer = 0.f;
        for (auto& [entId, ent] : _entities) {
          if (ent.type == ServerEntityType::Bydos) {
            spawnEnemyMissile(ent);
          }
        }
      }

      checkCollisions(destroyedEntities);

      for (auto entId : destroyedEntities) {
        destroyEntity(entId);
      }
    }

    void spawnPlayerMissile(const ConnectedClient& client) {
      ServerEntity missile;
      missile.id = _nextEntityId++;
      missile.type = ServerEntityType::PlayerMissile;
      missile.posX = client.posX + 33.f;
      missile.posY = client.posY + 6.f;
      missile.velX = 900.f;
      missile.velY = 0.f;
      missile.hp = 1;
      missile.maxHp = 1;
      missile.ownerId = client.id;

      _entities[missile.id] = missile;
      broadcastSpawnEntity(missile);
    }

    void spawnBydo() {
      ServerEntity bydo;
      bydo.id = _nextEntityId++;
      bydo.type = ServerEntityType::Bydos;
      bydo.posX = 1970.f;
      bydo.posY = static_cast<float>(80 + (rand() % 881));
      bydo.velX = -160.f;
      bydo.velY = 0.f;

      int hp = 2;
      if (_currentDifficulty == 0) hp = 1;
      else if (_currentDifficulty == 2) hp = 3;
      bydo.hp = hp;
      bydo.maxHp = hp;

      _entities[bydo.id] = bydo;
      broadcastSpawnEntity(bydo);
      if (bydo.maxHp > 1) {
        broadcastHealthUpdate(bydo.id, bydo.hp, bydo.maxHp);
      }
    }

    void spawnEnemyMissile(const ServerEntity& bydo) {
      ServerEntity miss;
      miss.id = _nextEntityId++;
      miss.type = ServerEntityType::EnemyMissile;
      miss.posX = bydo.posX - 10.f;
      miss.posY = bydo.posY + 14.f;

      float speed = 420.f;
      if (_currentDifficulty == 0) speed = 320.f;
      else if (_currentDifficulty == 2) speed = 520.f;

      miss.velX = -speed;
      miss.velY = 0.f;

      if (_currentDifficulty == 2) {
        float closestDist = 9999.f;
        const ConnectedClient* target = nullptr;
        for (const auto& [_, client] : _clients) {
          if (!client.dead && client.posX < bydo.posX - 40.f) {
            float dist = std::hypot(client.posX - bydo.posX, client.posY - bydo.posY);
            if (dist < closestDist) {
              closestDist = dist;
              target = &client;
            }
          }
        }
        if (target) {
          float angle = std::atan2(target->posY - bydo.posY, target->posX - bydo.posX);
          angle = std::clamp(angle, -0.52f, 0.52f);
          miss.velX = speed * std::cos(angle);
          miss.velY = speed * std::sin(angle);
        }
      }

      miss.hp = 1;
      miss.maxHp = 1;
      _entities[miss.id] = miss;
      broadcastSpawnEntity(miss);
    }

    void checkCollisions(std::vector<network::NetworkId>& destroyed) {
      for (auto& [entId, ent] : _entities) {
        if (std::find(destroyed.begin(), destroyed.end(), entId) != destroyed.end()) continue;

        if (ent.type == ServerEntityType::PlayerMissile) {
          for (auto& [targetId, target] : _entities) {
            if (target.type == ServerEntityType::Bydos && std::find(destroyed.begin(), destroyed.end(), targetId) == destroyed.end()) {
              if (std::abs(ent.posX - target.posX) < 33.f && std::abs(ent.posY - target.posY) < 33.f) {
                destroyed.push_back(entId);
                target.hp--;
                broadcastHealthUpdate(targetId, target.hp, target.maxHp);
                if (target.hp <= 0) {
                  destroyed.push_back(targetId);
                  maybeSpawnBonus(target.posX, target.posY);
                }
                break;
              }
            }
          }
        }

        if (ent.type == ServerEntityType::EnemyMissile) {
          for (auto& [clientId, client] : _clients) {
            if (!client.dead) {
              if (std::abs(ent.posX - client.posX) < 33.f && std::abs(ent.posY - client.posY) < 17.f) {
                destroyed.push_back(entId);
                client.hp--;
                broadcastHealthUpdate(client.id, client.hp, client.maxHp);
                if (client.hp <= 0) {
                  client.dead = true;
                  client.deathTimer = 1.5f;
                  broadcastHealthUpdate(client.id, 0, client.maxHp);
                }
                break;
              }
            }
          }
        }

        if (ent.type == ServerEntityType::Bydos) {
          for (auto& [clientId, client] : _clients) {
            if (!client.dead) {
              if (std::abs(ent.posX - client.posX) < 33.f && std::abs(ent.posY - client.posY) < 25.f) {
                destroyed.push_back(entId);
                client.hp--;
                broadcastHealthUpdate(client.id, client.hp, client.maxHp);
                if (client.hp <= 0) {
                  client.dead = true;
                  client.deathTimer = 1.5f;
                  broadcastHealthUpdate(client.id, 0, client.maxHp);
                }
                break;
              }
            }
          }
        }
      }
    }

    void maybeSpawnBonus(float x, float y) {
      int chance = 30;
      if (_currentDifficulty == 0) chance = 40;
      else if (_currentDifficulty == 2) chance = 20;

      if ((rand() % 100) < chance) {
        ServerEntity bonus;
        bonus.id = _nextEntityId++;
        bonus.type = static_cast<ServerEntityType>(4 + (rand() % 4));
        bonus.posX = x;
        bonus.posY = y;
        bonus.velX = -80.f;
        bonus.velY = 0.f;
        bonus.hp = 1;
        bonus.maxHp = 1;

        _entities[bonus.id] = bonus;
        broadcastSpawnEntity(bonus);
      }
    }

    void broadcastSpawnEntity(const ServerEntity& ent) {
      for (const auto& [id, client] : _clients) {
        sendSpawnEntity(ent, client.endpoint);
      }
    }

    void broadcastHealthUpdate(network::NetworkId netId, std::uint8_t hp, std::uint8_t maxHp) {
      for (const auto& [id, client] : _clients) {
        sendHealthUpdate(netId, hp, maxHp, client.endpoint);
      }
    }

    void destroyEntity(network::NetworkId entId) {
      _entities.erase(entId);
      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::DestroyEntity, _outgoingSeq++, sizeof(entId)};
      network::PacketStreamWriter writer;
      writer << header << entId;
      for (const auto& [id, client] : _clients) {
        _socket.send(writer.getBuffer(), client.endpoint);
      }
    }

    void broadcastGameState() {
      if (_clients.empty()) return;

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::GameStateSync, _outgoingSeq++, 0};
      network::PacketStreamWriter writer;
      writer << header;

      std::uint16_t totalCount = static_cast<std::uint16_t>(_clients.size() + _entities.size());
      writer << totalCount;

      for (const auto& [id, client] : _clients) {
        network::EntityStateData state{client.id, client.posX, client.posY, client.velX, client.velY};
        writer << state;
      }

      for (const auto& [id, ent] : _entities) {
        network::EntityStateData state{ent.id, ent.posX, ent.posY, ent.velX, ent.velY};
        writer << state;
      }

      for (const auto& [id, client] : _clients) {
        _socket.send(writer.getBuffer(), client.endpoint);
      }
    }

    network::INetworkSocket& _socket;
    std::unordered_map<network::NetworkId, ConnectedClient> _clients;
    std::unordered_map<network::NetworkId, ServerEntity> _entities;
    network::NetworkId _nextPlayerId{1000};
    network::NetworkId _nextEntityId{1};
    std::uint32_t _outgoingSeq{1};
    std::uint8_t _currentDifficulty{1};
    float _bydoSpawnTimer{0.f};
    float _enemyShootTimer{0.f};
  };

}

#endif // !SERVERNETWORKMANAGER_HPP
