#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
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
    float idleTime{0.0f};
  };

  class ServerNetworkManager {
  public:
    explicit ServerNetworkManager(network::INetworkSocket& socket) : _socket(socket) {}

    void update(float dt) {
      processIncomingPackets();
      removeTimedOutClients(dt);
      updatePhysics(dt);
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
          continue; // paquet malformé : on l'ignore, le serveur continue
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
        case network::PacketType::Disconnect: handleDisconnect(reader); break;
        default: break;
      }
    }

    void handleConnect(const network::Endpoint& sender) {
      network::NetworkId netId = _nextNetId++;
      ConnectedClient client{netId, sender, 200.0f + (netId * 50.0f), 300.0f, 0.0f, 0.0f, 0.0f};
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
    }

    void handleInput(network::PacketStreamReader& reader) {
      network::NetworkId netId;
      bool up, down, left, right;
      reader >> netId >> up >> down >> left >> right;

      auto it = _clients.find(netId);
      if (it != _clients.end()) {
        it->second.idleTime = 0.0f; // le client est vivant
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
      if (_clients.erase(netId) == 0) return;

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

    void updatePhysics(float dt) {
      for (auto& [id, client] : _clients) {
        client.posX += client.velX * dt;
        client.posY += client.velY * dt;
      }
    }

    void broadcastGameState() {
      if (_clients.empty()) return;

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::GameStateSync, _outgoingSeq++, 0};
      network::PacketStreamWriter writer;
      writer << header;

      auto count = static_cast<std::uint16_t>(_clients.size());
      writer << count;

      for (const auto& [id, client] : _clients) {
        network::EntityStateData state{client.id, client.posX, client.posY, client.velX, client.velY};
        writer << state;
      }

      for (const auto& [id, client] : _clients)
        _socket.send(writer.getBuffer(), client.endpoint);
    }

    network::INetworkSocket& _socket;
    std::unordered_map<network::NetworkId, ConnectedClient> _clients;
    network::NetworkId _nextNetId{1};
    std::uint32_t _outgoingSeq{1};
  };
}

#endif // !SERVERNETWORKMANAGER_HPP