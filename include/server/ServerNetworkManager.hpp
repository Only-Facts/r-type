#ifndef SERVERNETWORKMANAGER_HPP
#define SERVERNETWORKMANAGER_HPP

#include "network/INetworkSocket.hpp"
#include "network/Protocol.hpp"
#include "network/PacketStream.hpp"
#include <unordered_map>
#include <chrono>
#include <vector>

namespace rtype::server {
  struct ConnectedClient {
    network::NetworkId id;
    network::Endpoint endpoint;
    float posX{200.0f};
    float posY{300.0f};
    float velX{0.0f};
    float velY{0.0f};
    std::chrono::steady_clock::time_point lastActivity{std::chrono::steady_clock::now()};
  };

  class ServerNetworkManager {
  public:
    explicit ServerNetworkManager(network::INetworkSocket& socket) : _socket(socket) {}

    void update(float dt) {
      processIncomingPackets();
      checkTimeouts();
      updatePhysics(dt);
      broadcastGameState();
    }

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

        network::NetworkId senderNetId = getClientNetIdByEndpoint(sender);
        if (senderNetId != 0) {
          auto it = _clients.find(senderNetId);
          if (it != _clients.end()) {
            it->second.lastActivity = std::chrono::steady_clock::now();
          }
        }

        switch (header.type) {
          case network::PacketType::ConnectReq:
            handleConnect(sender);
            break;
          case network::PacketType::Disconnect:
            if (senderNetId != 0) {
              removeClient(senderNetId);
            }
            break;
          case network::PacketType::PlayerInput:
            handleInput(reader);
            break;
          default:
            break;
        }
      }
    }

    network::NetworkId getClientNetIdByEndpoint(const network::Endpoint& endpoint) const {
      for (const auto& [id, client] : _clients) {
        if (client.endpoint.address == endpoint.address && client.endpoint.port == endpoint.port) {
          return id;
        }
      }
      return 0;
    }

    void handleConnect(const network::Endpoint& sender) {
      network::NetworkId netId = _nextNetId++;
      ConnectedClient client{netId, sender, 200.0f + (netId * 50.0f), 300.0f, 0.0f, 0.0f, std::chrono::steady_clock::now()};
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

    void checkTimeouts() {
      auto now = std::chrono::steady_clock::now();
      std::vector<network::NetworkId> timedOutClients;

      for (const auto& [id, client] : _clients) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - client.lastActivity).count();
        if (elapsed > 5000) {
          timedOutClients.push_back(id);
        }
      }

      for (network::NetworkId id : timedOutClients) {
        removeClient(id);
      }
    }

    void removeClient(network::NetworkId netId) {
      auto it = _clients.find(netId);
      if (it == _clients.end()) return;

      _clients.erase(it);

      network::PacketHeader header{network::PROTOCOL_MAGIC, network::PacketType::DestroyEntity, _outgoingSeq++, sizeof(netId)};
      network::PacketStreamWriter writer;
      writer << header << netId;

      for (const auto& [id, client] : _clients) {
        _socket.send(writer.getBuffer(), client.endpoint);
      }
    }

    void handleInput(network::PacketStreamReader& reader) {
      network::NetworkId netId;
      bool up, down, left, right;
      reader >> netId >> up >> down >> left >> right;

      auto it = _clients.find(netId);
      if (it != _clients.end()) {
        float speed = 300.0f;
        it->second.velX = 0.0f;
        it->second.velY = 0.0f;
        if (up) it->second.velY -= speed;
        if (down) it->second.velY += speed;
        if (left) it->second.velX -= speed;
        if (right) it->second.velX += speed;
      }
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
