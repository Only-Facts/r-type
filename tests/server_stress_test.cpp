#include "ServerNetworkManager.hpp"
#include <cassert>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace rtype;

class StressMockSocket final : public network::INetworkSocket {
public:
  struct Datagram {
    network::PacketData data;
    network::Endpoint endpoint;
  };

  void bind(unsigned short = 0) override {}
  void setNonBlocking(bool) override {}
  unsigned short getLocalPort() const override { return 4242; }

  void send(const network::PacketData& data,
            const network::Endpoint& endpoint) override {
    sent.push_back({data, endpoint});
  }

  bool receive(network::PacketData& data,
               network::Endpoint& endpoint) override {
    if (incoming.empty()) return false;
    data = std::move(incoming.front().data);
    endpoint = std::move(incoming.front().endpoint);
    incoming.pop_front();
    return true;
  }

  void enqueue(network::PacketData data, network::Endpoint endpoint) {
    incoming.push_back({std::move(data), std::move(endpoint)});
  }

  std::deque<Datagram> incoming;
  std::vector<Datagram> sent;
};

static network::PacketData makePacket(network::PacketType type,
                                      const network::PacketData& payload) {
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, type, 1,
    static_cast<std::uint16_t>(payload.size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto packet = writer.getBuffer();
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

static network::PacketData makeRoomCreate() {
  network::PacketStreamWriter payload;
  payload << std::uint8_t{4} << std::uint8_t{1};
  return makePacket(network::PacketType::RoomCreateReq, payload.getBuffer());
}

static network::PacketData makeRoomJoin(std::uint32_t roomId) {
  network::PacketStreamWriter payload;
  payload << roomId << std::uint8_t{0xFF} << std::uint8_t{0xFF}
          << network::NetworkId{0};
  return makePacket(network::PacketType::RoomJoinReq, payload.getBuffer());
}

static network::PacketData makeAck(std::uint32_t sequence) {
  network::PacketStreamWriter payload;
  payload << sequence;
  return makePacket(network::PacketType::Ack, payload.getBuffer());
}

static void testSixteenRoomsAndSixtyPlayers() {
  StressMockSocket socket;
  server::ServerNetworkManager manager(socket, 16, 4);
  std::vector<std::uint32_t> roomIds;

  for (std::uint16_t room = 0; room < 15; ++room) {
    const network::Endpoint owner{
      "127.0.0.1", static_cast<std::uint16_t>(6000 + room)};
    socket.enqueue(makeRoomCreate(), owner);
    manager.update(0.0f);
  }
  assert(manager.roomCount() == 16);

  for (std::uint16_t room = 0; room < 15; ++room) {
    const std::uint32_t roomId = static_cast<std::uint32_t>(room) + 2;
    roomIds.push_back(roomId);
    for (std::uint16_t player = 0; player < 4; ++player) {
      const auto port = static_cast<std::uint16_t>(6100 + room * 4 + player);
      const network::Endpoint endpoint{"127.0.0.1", port};
      socket.enqueue(makeRoomJoin(roomId), endpoint);
      manager.update(0.017f);
    }
  }

  auto stats = manager.stats();
  assert(stats.rooms == 16);
  assert(stats.players == 60);
  assert(stats.reliablePending > 0);
  assert(stats.reliablePending < server::ReliableUdpQueue::MAX_PENDING_PACKETS);
  assert(stats.reliableQueueDrops == 0);
  assert(stats.rateLimitedPackets == 0);

  std::set<std::pair<std::uint16_t, std::uint32_t>> acknowledgements;
  for (const auto& sent : socket.sent) {
    network::PacketStreamReader reader(sent.data);
    network::PacketHeader header{};
    reader >> header;
    if (server::ReliableUdpQueue::isReliable(header.type))
      acknowledgements.emplace(sent.endpoint.port, header.sequence);
  }
  for (const auto& [port, sequence] : acknowledgements) {
    socket.enqueue(makeAck(sequence),
      {"127.0.0.1", port});
  }
  manager.update(0.017f);
  stats = manager.stats();
  assert(stats.reliablePending == 0);
  assert(stats.reliableAcknowledged == acknowledgements.size());
  assert(stats.players == 60);
  assert(stats.reliableQueueDrops == 0);
}

int main() {
  testSixteenRoomsAndSixtyPlayers();
  return 0;
}
