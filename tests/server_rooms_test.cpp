#include "ServerNetworkManager.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace rtype;

class MockSocket final : public network::INetworkSocket {
public:
  struct SentPacket {
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
    std::lock_guard<std::mutex> lock(incomingMutex);
    if (incoming.empty()) return false;
    data = std::move(incoming.front().data);
    endpoint = std::move(incoming.front().endpoint);
    incoming.pop_front();
    return true;
  }

  void enqueue(network::PacketData data, network::Endpoint endpoint) {
    std::lock_guard<std::mutex> lock(incomingMutex);
    incoming.push_back({std::move(data), std::move(endpoint)});
  }

  void clearSent() { sent.clear(); }

  std::mutex incomingMutex;
  std::deque<SentPacket> incoming;
  std::vector<SentPacket> sent;
};

static network::PacketData packet(network::PacketType type,
                                  const network::PacketData& payload = {},
                                  std::uint32_t sequence = 1) {
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, type, sequence,
    static_cast<std::uint16_t>(payload.size())
  };
  network::PacketStreamWriter writer;
  writer << header;
  auto result = writer.getBuffer();
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

static network::PacketData payloadRoomCreate(std::uint8_t capacity,
                                             std::uint8_t difficulty) {
  network::PacketStreamWriter writer;
  writer << capacity << difficulty;
  return writer.getBuffer();
}

static network::PacketData payloadRoomJoin(std::uint32_t roomId,
                                           std::uint8_t difficulty = 1) {
  network::PacketStreamWriter writer;
  writer << roomId << difficulty << std::uint8_t{0xFF}
         << network::NetworkId{0};
  return writer.getBuffer();
}

static network::PacketType packetType(const MockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  reader >> header;
  return header.type;
}

static std::uint16_t snapshotEntityCount(
    const MockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  std::uint32_t tick{};
  std::uint16_t count{};
  reader >> header >> tick >> count;
  return count;
}

static std::uint8_t roomStatus(const MockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  std::uint32_t roomId{};
  std::uint8_t status{};
  reader >> header >> roomId >> status;
  return status;
}

static void testDefaultRoomCompatibility() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint client{"127.0.0.1", 5000};
  socket.enqueue(packet(network::PacketType::ConnectReq), client);
  manager.update(0.016f);
  assert(manager.roomCount() == 1);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  }));
}

static void testCreateListAndJoinRooms() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint owner{"127.0.0.1", 5001};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 2)), owner);
  manager.update(0.016f);
  assert(manager.roomCount() == 2);
  assert(socket.sent.size() == 1);
  assert(packetType(socket.sent.front()) == network::PacketType::RoomCreateAck);
  assert(roomStatus(socket.sent.front()) == 0);
  const auto createdRoomId = [&socket]() {
    network::PacketStreamReader reader(socket.sent.front().data);
    network::PacketHeader header{};
    std::uint32_t roomId{};
    std::uint8_t status{};
    reader >> header >> roomId >> status;
    return roomId;
  }();
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 2)), owner);
  manager.update(0.016f);
  assert(manager.roomCount() == 2);
  network::PacketStreamReader duplicateCreate(socket.sent.front().data);
  network::PacketHeader duplicateHeader{};
  std::uint32_t duplicateRoomId{};
  std::uint8_t duplicateStatus{};
  duplicateCreate >> duplicateHeader >> duplicateRoomId >> duplicateStatus;
  assert(duplicateRoomId == createdRoomId && duplicateStatus == 0);

  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomListReq), owner);
  manager.update(0.016f);
  assert(socket.sent.size() == 1);
  assert(packetType(socket.sent.front()) == network::PacketType::RoomListAck);
  {
    network::PacketStreamReader reader(socket.sent.front().data);
    network::PacketHeader header{};
    std::uint16_t count{};
    reader >> header >> count;
    assert(count == 2);
  }

  socket.clearSent();
  const network::Endpoint first{"127.0.0.1", 5002};
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), first);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::RoomJoinAck &&
           roomStatus(sent) == 0;
  }));
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), first);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  }));
}

static void testRoomCapacityAndIsolation() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint creator{"127.0.0.1", 5100};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), creator);
  manager.update(0.016f);
  socket.clearSent();

  const network::Endpoint first{"127.0.0.1", 5101};
  const network::Endpoint second{"127.0.0.1", 5102};
  const network::Endpoint third{"127.0.0.1", 5103};
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), first);
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), second);
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), third);

  network::PacketData defaultConnectPayload;
  socket.enqueue(packet(network::PacketType::ConnectReq,
                        defaultConnectPayload), creator);
  manager.update(0.016f);

  bool rejectedThird = false;
  for (const auto& sent : socket.sent) {
    if (packetType(sent) == network::PacketType::RoomJoinAck &&
        sent.endpoint.port == third.port) {
      rejectedThird = roomStatus(sent) == 2;
    }
  }
  assert(rejectedThird);

  const auto hasSync = [&socket](unsigned short port) {
    return std::any_of(socket.sent.begin(), socket.sent.end(),
      [port](const auto& sent) {
        return sent.endpoint.port == port &&
               packetType(sent) == network::PacketType::GameStateSyncPacked;
      });
  };
  assert(hasSync(first.port));
  assert(hasSync(second.port));
  assert(!hasSync(third.port));
  assert(hasSync(creator.port));
  std::uint32_t previousSequence = 0;
  network::PacketData previousPacket;
  for (const auto& sent : socket.sent) {
    network::PacketStreamReader reader(sent.data);
    network::PacketHeader header{};
    reader >> header;
    if (header.sequence == previousSequence) {
      assert(sent.data == previousPacket);
      continue;
    }
    assert(header.sequence > previousSequence);
    previousSequence = header.sequence;
    previousPacket = sent.data;
  }
  for (const auto& sent : socket.sent) {
    if (packetType(sent) != network::PacketType::GameStateSyncPacked) continue;
    if (sent.endpoint.port == first.port || sent.endpoint.port == second.port)
      assert(snapshotEntityCount(sent) == 2);
    if (sent.endpoint.port == creator.port)
      assert(snapshotEntityCount(sent) == 1);
  }
}

struct ConnectInfo {
  network::NetworkId playerId{0};
  std::uint64_t sessionToken{0};
};

static ConnectInfo connectInfo(const MockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  ConnectInfo result;
  std::uint8_t difficulty{};
  reader >> header >> result.playerId >> difficulty >> result.sessionToken;
  return result;
}

static void testLeaveAndRejoinRoom() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint owner{"127.0.0.1", 5250};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), owner);
  manager.update(0.016f);
  socket.clearSent();

  const network::Endpoint player{"127.0.0.1", 5251};
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), player);
  manager.update(0.016f);
  auto ack = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  });
  assert(ack != socket.sent.end());
  const auto connected = connectInfo(*ack);

  socket.clearSent();
  network::PacketStreamWriter leavePayload;
  leavePayload << connected.playerId << connected.sessionToken;
  socket.enqueue(packet(network::PacketType::RoomLeaveReq,
                        leavePayload.getBuffer()), player);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::RoomLeaveAck &&
           roomStatus(sent) == 0;
  }));

  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomLeaveReq,
                        leavePayload.getBuffer()), player);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::RoomLeaveAck &&
           roomStatus(sent) == 0;
  }));

  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), player);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  }));
}

static void testEmptyRoomStopsAndResetsSimulation() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint creator{"127.0.0.1", 5280};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), creator);
  manager.update(0.016f);
  const network::Endpoint player{"127.0.0.1", 5281};
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), player);
  manager.update(0.016f);
  auto ack = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  });
  assert(ack != socket.sent.end());
  const auto connected = connectInfo(*ack);
  const auto playerId = connected.playerId;

  socket.clearSent();
  for (int tick = 0; tick < 5; ++tick) manager.update(0.25f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == static_cast<network::PacketType>(0x12);
  }));

  socket.clearSent();
  network::PacketStreamWriter leavePayload;
  leavePayload << playerId << connected.sessionToken;
  socket.enqueue(packet(network::PacketType::RoomLeaveReq,
                        leavePayload.getBuffer()), player);
  manager.update(0.016f);
  socket.clearSent();
  for (int tick = 0; tick < 8; ++tick) manager.update(0.25f);

  socket.clearSent();
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(2)), player);
  manager.update(0.016f);
  assert(!std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == static_cast<network::PacketType>(0x12);
  }));
}

static void testDefaultRoomCapacity() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket, 1, 1);
  const network::Endpoint first{"127.0.0.1", 5260};
  const network::Endpoint second{"127.0.0.1", 5261};
  socket.enqueue(packet(network::PacketType::ConnectReq), first);
  socket.enqueue(packet(network::PacketType::ConnectReq), second);
  manager.update(0.016f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectReject;
  }));
}

static void testNonBooleanInputBytesAreRejected() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint client{"127.0.0.1", 5265};
  socket.enqueue(packet(network::PacketType::ConnectReq), client);
  manager.update(0.016f);
  auto ack = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::ConnectAck;
  });
  assert(ack != socket.sent.end());
  const auto connected = connectInfo(*ack);

  network::PacketStreamWriter rightPayload;
  rightPayload << connected.playerId << connected.sessionToken
               << std::uint8_t{0} << std::uint8_t{0}
               << std::uint8_t{0} << std::uint8_t{1} << std::uint8_t{0};
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        rightPayload.getBuffer(), 2), client);
  manager.update(0.034f);
  auto moving = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::GameStateSyncDelta;
  });
  assert(moving != socket.sent.end());

  network::PacketStreamWriter neutralPayload;
  neutralPayload << connected.playerId << connected.sessionToken
                 << std::uint8_t{0} << std::uint8_t{0}
                 << std::uint8_t{0} << std::uint8_t{0} << std::uint8_t{0};
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        neutralPayload.getBuffer(), 3), client);
  manager.update(0.034f);
  auto stopped = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::GameStateSyncDelta;
  });
  assert(stopped != socket.sent.end());
  network::PacketStreamReader stoppedReader(stopped->data);
  network::PacketHeader stoppedHeader{};
  std::uint32_t stoppedTick{};
  std::uint16_t stoppedCount{};
  network::PackedEntityStateData stoppedState{};
  stoppedReader >> stoppedHeader >> stoppedTick >> stoppedCount >> stoppedState;
  assert(stoppedCount == 1);
  assert(network::unpackEntityState(stoppedState).velX == 0.0f);

  network::PacketStreamWriter invalidPayload;
  invalidPayload << connected.playerId << connected.sessionToken
                 << std::uint8_t{2} << std::uint8_t{0}
                 << std::uint8_t{0} << std::uint8_t{0} << std::uint8_t{0};
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        invalidPayload.getBuffer(), 4), client);
  manager.update(0.034f);
  const auto changedState = std::find_if(
    socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
      return packetType(sent) == network::PacketType::GameStateSyncDelta ||
             packetType(sent) == network::PacketType::GameStateSyncPacked;
    });
  assert(changedState == socket.sent.end());
  assert(manager.stats().malformedPackets == 1);

  network::PacketStreamWriter forgedPayload;
  forgedPayload << connected.playerId << (connected.sessionToken + 1)
                << std::uint8_t{0} << std::uint8_t{0}
                << std::uint8_t{0} << std::uint8_t{0} << std::uint8_t{0};
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        forgedPayload.getBuffer(), 5), client);
  manager.update(0.034f);
  assert(manager.stats().rejectedPackets == 1);
  const auto forgedState = std::find_if(
    socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
      return packetType(sent) == network::PacketType::GameStateSyncDelta ||
             packetType(sent) == network::PacketType::GameStateSyncPacked;
    });
  assert(forgedState == socket.sent.end());
}

static void testMalformedPacketIsIgnored() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint client{"127.0.0.1", 5270};
  auto malformed = packet(network::PacketType::ConnectReq);
  malformed[sizeof(network::PacketHeader) - 1] = 1;
  socket.enqueue(std::move(malformed), client);
  manager.update(0.016f);
  assert(socket.sent.empty());
  assert(manager.stats().malformedPackets == 1);
}


static void testPlayerInputRoutingRejectsUnknownAndMalformedPeers() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint unknown{"127.0.0.1", 5280};

  network::PacketStreamWriter validPayload;
  validPayload << network::NetworkId{1000} << std::uint64_t{1}
               << std::uint8_t{0} << std::uint8_t{0}
               << std::uint8_t{0} << std::uint8_t{0} << std::uint8_t{0};
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        validPayload.getBuffer()), unknown);
  manager.update(0.016f);
  assert(manager.stats().rejectedPackets == 1);

  socket.enqueue(packet(network::PacketType::PlayerInput), unknown);
  manager.update(0.016f);
  assert(manager.stats().malformedPackets == 1);
}

static void testEmptyRoomCleanup() {
  MockSocket socket;
  server::ServerNetworkManager manager(
    socket, 16, 4, std::chrono::milliseconds(1));
  const network::Endpoint owner{"127.0.0.1", 5300};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), owner);
  manager.update(0.016f);
  assert(manager.roomCount() == 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  manager.update(0.016f);
  assert(manager.roomCount() == 1);
}

static void testReceiveThreadDoesNotOwnSimulationState() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint owner{"127.0.0.1", 5400};
  manager.startNetworkThread();
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), owner);
  for (int attempt = 0; attempt < 100 && manager.roomCount() == 1; ++attempt) {
    manager.update(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  manager.stopNetworkThread();
  assert(manager.roomCount() == 2);
  assert(!socket.sent.empty());
  assert(packetType(socket.sent.front()) == network::PacketType::RoomCreateAck);
}


static network::PacketData acknowledgement(std::uint32_t sequence) {
  network::PacketStreamWriter payload;
  payload << sequence;
  return packet(network::PacketType::Ack, payload.getBuffer());
}

static void testServerReliableAcknowledgementFlow() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint owner{"127.0.0.1", 5450};
  const network::Endpoint stranger{"127.0.0.1", 5451};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), owner);
  manager.update(0.016f);
  assert(manager.pendingReliablePackets() == 1);
  assert(socket.sent.size() == 1);

  network::PacketStreamReader reader(socket.sent.front().data);
  network::PacketHeader header{};
  reader >> header;
  assert(header.type == network::PacketType::RoomCreateAck);

  socket.enqueue(acknowledgement(header.sequence), stranger);
  manager.update(0.016f);
  assert(manager.pendingReliablePackets() == 1);

  manager.update(0.151f);
  assert(socket.sent.size() == 2);
  assert(socket.sent[0].data == socket.sent[1].data);
  assert(manager.retransmittedPackets() == 1);

  socket.enqueue(acknowledgement(header.sequence), owner);
  manager.update(0.016f);
  assert(manager.pendingReliablePackets() == 0);
  assert(manager.stats().reliableAcknowledged == 1);
}

static void testUnknownRoomIsRejected() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint client{"127.0.0.1", 5200};
  socket.enqueue(packet(network::PacketType::RoomJoinReq,
                        payloadRoomJoin(999)), client);
  manager.update(0.016f);
  assert(socket.sent.size() == 1);
  assert(packetType(socket.sent.front()) == network::PacketType::RoomJoinAck);
  assert(roomStatus(socket.sent.front()) == 1);
}

int main() {
  testDefaultRoomCompatibility();
  testCreateListAndJoinRooms();
  testRoomCapacityAndIsolation();
  testLeaveAndRejoinRoom();
  testEmptyRoomStopsAndResetsSimulation();
  testDefaultRoomCapacity();
  testNonBooleanInputBytesAreRejected();
  testMalformedPacketIsIgnored();
  testPlayerInputRoutingRejectsUnknownAndMalformedPeers();
  testEmptyRoomCleanup();
  testReceiveThreadDoesNotOwnSimulationState();
  testServerReliableAcknowledgementFlow();
  testUnknownRoomIsRejected();
  std::cout << "server room tests passed\n";
  return 0;
}
