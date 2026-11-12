#include "ServerNetworkManager.hpp"
#include "engine/ecs/Registry.hpp"
#include "network/ClientNetworkManager.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

using namespace rtype;
using SessionState = client::ClientNetworkManager::SessionState;
using RoomResult = client::ClientNetworkManager::RoomResult;
using RoomInfo = client::ClientNetworkManager::RoomInfo;

namespace {
  constexpr unsigned short SERVER_PORT = 4242;
  const network::Endpoint SERVER{"127.0.0.1", SERVER_PORT};

  class MockSocket final : public network::INetworkSocket {
  public:
    void bind(unsigned short = 0) override {}
    void setNonBlocking(bool) override {}
    unsigned short getLocalPort() const override { return 0; }

    void send(const network::PacketData& data, const network::Endpoint& endpoint) override {
      sent.emplace_back(data, endpoint);
    }

    bool receive(network::PacketData& data, network::Endpoint& endpoint) override {
      if (incoming.empty()) return false;
      data = std::move(incoming.front().first);
      endpoint = std::move(incoming.front().second);
      incoming.pop_front();
      return true;
    }

    std::deque<std::pair<network::PacketData, network::Endpoint>> incoming;
    std::vector<std::pair<network::PacketData, network::Endpoint>> sent;
  };

  struct Player {
    explicit Player(unsigned short port)
      : endpoint{"127.0.0.1", port}, network(socket, registry) {
      network.setOnRoomList([this](RoomResult result, const std::vector<RoomInfo>& list) {
        listResult = result;
        rooms = list;
      });
      network.setOnRoomCreated([this](RoomResult result, std::uint32_t roomId) { created = {result, roomId}; });
      network.setOnRoomJoined([this](RoomResult result, std::uint32_t roomId) { joined = {result, roomId}; });
      network.setOnRoomLeft([this](RoomResult result, std::uint32_t roomId) { left = {result, roomId}; });
    }

    std::size_t remoteEntities() {
      std::size_t count = 0;
      registry.each<engine::NetworkIdComponent>([&](engine::Entity, engine::NetworkIdComponent&) { ++count; });
      return count;
    }

    const RoomInfo* room(std::uint32_t id) const {
      for (const auto& info : rooms)
        if (info.id == id) return &info;
      return nullptr;
    }

    network::Endpoint endpoint;
    MockSocket socket;
    engine::Registry registry;
    client::ClientNetworkManager network;
    std::optional<RoomResult> listResult;
    std::vector<RoomInfo> rooms;
    std::optional<std::pair<RoomResult, std::uint32_t>> created;
    std::optional<std::pair<RoomResult, std::uint32_t>> joined;
    std::optional<std::pair<RoomResult, std::uint32_t>> left;
  };

  class Network {
  public:
    explicit Network(std::chrono::milliseconds emptyRoomTtl)
      : server(serverSocket, 16, 4, emptyRoomTtl) {}

    Player& addPlayer() {
      players.push_back(std::make_unique<Player>(static_cast<unsigned short>(50000 + players.size())));
      return *players.back();
    }

    void pump(int rounds = 4) {
      for (int round = 0; round < rounds; ++round) {
        for (auto& player : players) {
          player->network.update(0.0f);
          for (auto& [data, to] : player->socket.sent) serverSocket.incoming.emplace_back(data, player->endpoint);
          player->socket.sent.clear();
        }
        server.update(0.0f);
        for (auto& [data, to] : serverSocket.sent) {
          for (auto& player : players)
            if (player->endpoint.port == to.port) player->socket.incoming.emplace_back(data, SERVER);
        }
        serverSocket.sent.clear();
      }
    }

    MockSocket serverSocket;
    server::ServerNetworkManager server;
    std::vector<std::unique_ptr<Player>> players;
  };

  void join(Network& net, Player& player, std::uint32_t roomId) {
    player.joined.reset();
    assert(player.network.joinRoom(SERVER.address, SERVER_PORT, roomId, 1, 0));
    net.pump();
  }

  void testCreateJoinFullAndUnknownRooms() {
    Network net(std::chrono::seconds(30));
    Player& alice = net.addPlayer();
    Player& bob = net.addPlayer();
    Player& carol = net.addPlayer();
    for (auto& player : net.players) player->network.openLobby(SERVER.address, SERVER_PORT);

    assert(alice.network.requestRooms());
    net.pump();
    assert(alice.listResult == RoomResult::Success);
    assert(alice.rooms.size() == 1);
    assert(alice.room(1) && alice.room(1)->players == 0 && alice.room(1)->capacity == 4);

    assert(alice.network.createRoom(2, 2));
    net.pump();
    assert(alice.created && alice.created->first == RoomResult::Success);
    const std::uint32_t roomId = alice.created->second;
    assert(roomId >= 2);

    alice.network.requestRooms();
    net.pump();
    assert(alice.rooms.size() == 2);
    assert(alice.room(roomId)->capacity == 2 && alice.room(roomId)->players == 0);
    assert(alice.room(roomId)->difficulty == 2);

    join(net, alice, roomId);
    assert(alice.joined == std::make_pair(RoomResult::Success, roomId));
    assert(alice.network.getState() == SessionState::InGame);
    assert(alice.network.getServerDifficulty() == std::optional<std::uint8_t>{2});

    join(net, bob, roomId);
    assert(bob.network.getState() == SessionState::InGame);
    assert(alice.remoteEntities() == 2);
    assert(bob.remoteEntities() == 2);

    join(net, carol, roomId);
    assert(carol.joined == std::make_pair(RoomResult::Full, roomId));
    assert(carol.network.getState() == SessionState::Lobby);

    join(net, carol, 999);
    assert(carol.joined == std::make_pair(RoomResult::NotFound, std::uint32_t{999}));
    assert(carol.network.getState() == SessionState::Lobby);

    carol.network.requestRooms();
    net.pump();
    assert(carol.room(roomId)->players == 2 && carol.room(roomId)->state == 2);
  }

  void testLeaveThenJoinAnotherRoom() {
    Network net(std::chrono::seconds(30));
    Player& alice = net.addPlayer();
    Player& bob = net.addPlayer();
    alice.network.openLobby(SERVER.address, SERVER_PORT);
    bob.network.openLobby(SERVER.address, SERVER_PORT);

    alice.network.createRoom(4, 0);
    net.pump();
    const std::uint32_t roomId = alice.created->second;
    join(net, alice, roomId);
    join(net, bob, roomId);
    assert(alice.remoteEntities() == 2);

    assert(alice.network.leaveRoom());
    assert(alice.network.getState() == SessionState::Lobby);
    assert(alice.remoteEntities() == 0);
    net.pump();
    assert(alice.left == std::make_pair(RoomResult::Success, roomId));
    assert(!alice.network.isLeaving());
    assert(alice.remoteEntities() == 0);
    assert(bob.remoteEntities() == 1);

    alice.network.requestRooms();
    net.pump();
    assert(alice.room(roomId)->players == 1);

    join(net, alice, 1);
    assert(alice.joined == std::make_pair(RoomResult::Success, std::uint32_t{1}));
    assert(alice.network.getState() == SessionState::InGame);
    assert(alice.remoteEntities() == 1);
    assert(bob.remoteEntities() == 1);
    assert(alice.network.getLocalNetId() != 0);

    assert(alice.network.leaveRoom());
    net.pump();
    join(net, alice, roomId);
    assert(alice.network.getState() == SessionState::InGame);
    assert(alice.remoteEntities() == 2);
    assert(bob.remoteEntities() == 2);
  }

  void testExpiredRoomIsReportedAsNotFound() {
    Network net(std::chrono::milliseconds(20));
    Player& alice = net.addPlayer();
    alice.network.openLobby(SERVER.address, SERVER_PORT);
    alice.network.createRoom(2, 1);
    net.pump();
    const std::uint32_t roomId = alice.created->second;

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    net.pump();
    join(net, alice, roomId);
    assert(alice.joined == std::make_pair(RoomResult::NotFound, roomId));
    assert(alice.network.getState() == SessionState::Lobby);
  }
}

int main() {
  testCreateJoinFullAndUnknownRooms();
  testLeaveThenJoinAnotherRoom();
  testExpiredRoomIsReportedAsNotFound();
  return 0;
}
