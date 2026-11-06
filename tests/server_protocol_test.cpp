#include "ReliableUdpQueue.hpp"
#include "network/PacketStream.hpp"
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

using namespace rtype;

class ProtocolMockSocket final : public network::INetworkSocket {
public:
  struct Sent {
    network::PacketData packet;
    network::Endpoint endpoint;
  };

  void bind(unsigned short = 0) override {}
  void setNonBlocking(bool) override {}
  unsigned short getLocalPort() const override { return 4242; }
  void send(const network::PacketData& packet,
            const network::Endpoint& endpoint) override {
    sent.push_back({packet, endpoint});
  }
  bool receive(network::PacketData&, network::Endpoint&) override {
    return false;
  }

  std::vector<Sent> sent;
};

static network::PacketData makePacket(network::PacketType type,
                                      std::uint32_t sequence,
                                      const network::PacketData& payload = {}) {
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, type, sequence,
    static_cast<std::uint16_t>(payload.size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto packet = writer.getBuffer();
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

static void testLittleEndianWireFormat() {
  network::PacketHeader header{
    0x5254, network::PacketType::RoomCreateReq, 0x01020304, 0x0506
  };
  network::PacketStreamWriter writer;
  writer << header;
  const std::vector<std::uint8_t> expected{
    0x54, 0x52, 0x42, 0x04, 0x03, 0x02, 0x01, 0x06, 0x05
  };
  assert(writer.getBuffer() == expected);

  network::PacketStreamReader reader(writer.getBuffer());
  network::PacketHeader decoded{};
  reader >> decoded;
  assert(decoded.magic == header.magic);
  assert(decoded.type == header.type);
  assert(decoded.sequence == header.sequence);
  assert(decoded.payloadSize == header.payloadSize);
}

static void testPackedSnapshots() {
  const network::EntityStateData source{
    1234, 1024.125f, 512.25f, -900.0f, 123.5f
  };
  const auto packed = network::packEntityState(source);
  const auto unpacked = network::unpackEntityState(packed);
  assert(sizeof(network::PackedEntityStateData) == 12);
  assert(sizeof(network::EntityStateData) == 20);
  assert(unpacked.netId == source.netId);
  assert(std::abs(unpacked.posX - source.posX) <= 0.0625f);
  assert(std::abs(unpacked.posY - source.posY) <= 0.0625f);
  assert(std::abs(unpacked.velX - source.velX) <= 0.125f);
  assert(std::abs(unpacked.velY - source.velY) <= 0.125f);

  network::PacketStreamWriter writer;
  writer << packed;
  network::PacketStreamReader reader(writer.getBuffer());
  network::PackedEntityStateData decoded{};
  reader >> decoded;
  assert(decoded.netId == packed.netId);
  assert(decoded.posX == packed.posX);
  assert(decoded.posY == packed.posY);
  assert(decoded.velX == packed.velX);
  assert(decoded.velY == packed.velY);
}

static void testReliableUdpAcknowledgements() {
  ProtocolMockSocket socket;
  server::ServerMetrics metrics;
  server::ReliableUdpQueue queue(&metrics);
  const network::Endpoint peer{"127.0.0.1", 5000};
  const network::Endpoint other{"127.0.0.1", 5001};
  const auto packet = makePacket(network::PacketType::SpawnEntity, 42);

  assert(queue.send(socket, packet, peer));
  assert(socket.sent.size() == 1);
  assert(queue.pendingCount() == 1);
  assert(queue.pendingBytes() == packet.size());
  assert(!queue.acknowledge(other, 42));
  assert(queue.pendingCount() == 1);

  queue.update(socket, 0.149f);
  assert(socket.sent.size() == 1);
  queue.update(socket, 0.002f);
  assert(socket.sent.size() == 2);
  assert(queue.retransmittedCount() == 1);
  assert(queue.acknowledge(peer, 42));
  assert(queue.pendingCount() == 0);
  assert(queue.pendingBytes() == 0);
  assert(queue.acknowledgedCount() == 1);
  assert(metrics.sentDatagrams.load() == 2);
  assert(metrics.sentBytes.load() == packet.size() * 2);
}

static void testReliableQueueTimeoutAndCancellation() {
  ProtocolMockSocket socket;
  server::ReliableUdpQueue queue;
  const network::Endpoint peer{"127.0.0.1", 5100};
  const auto packet = makePacket(network::PacketType::ConnectAck, 77);
  assert(queue.send(socket, packet, peer));
  for (int tick = 0; tick < 10; ++tick) queue.update(socket, 1.0f);
  assert(queue.pendingCount() == 0);
  assert(queue.timedOutCount() == 1);
  assert(queue.retransmittedCount() == server::ReliableUdpQueue::MAX_RETRIES);

  assert(queue.send(socket, packet, peer));
  queue.cancelEndpoint(peer);
  assert(queue.pendingCount() == 0);
  assert(queue.pendingBytes() == 0);
}

static void testUnreliableSnapshotsAreNotQueued() {
  ProtocolMockSocket socket;
  server::ReliableUdpQueue queue;
  const network::Endpoint peer{"127.0.0.1", 5200};
  const auto packet = makePacket(network::PacketType::GameStateSyncPacked, 1);
  assert(queue.send(socket, packet, peer));
  assert(socket.sent.size() == 1);
  assert(queue.pendingCount() == 0);
}

int main() {
  testLittleEndianWireFormat();
  testPackedSnapshots();
  testReliableUdpAcknowledgements();
  testReliableQueueTimeoutAndCancellation();
  testUnreliableSnapshotsAreNotQueued();
}
