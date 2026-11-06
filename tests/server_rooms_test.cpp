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
                                  const network::PacketData& payload = {}) {
  network::PacketHeader header{
    network::PROTOCOL_MAGIC, type, 1,
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
  std::uint16_t count{};
  reader >> header >> count;
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

  socket.clearSent();
  manager.update(0.016f);
  const auto hasSync = [&socket](unsigned short port) {
    return std::any_of(socket.sent.begin(), socket.sent.end(),
      [port](const auto& sent) {
        return sent.endpoint.port == port &&
               packetType(sent) == network::PacketType::GameStateSync;
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
      // A room broadcast reuses the same datagram for every recipient.
      assert(sent.data == previousPacket);
      continue;
    }
    assert(header.sequence > previousSequence);
    previousSequence = header.sequence;
    previousPacket = sent.data;
  }
  for (const auto& sent : socket.sent) {
    if (packetType(sent) != network::PacketType::GameStateSync) continue;
    if (sent.endpoint.port == first.port || sent.endpoint.port == second.port)
      assert(snapshotEntityCount(sent) == 2);
    if (sent.endpoint.port == creator.port)
      assert(snapshotEntityCount(sent) == 1);
  }
}

static network::NetworkId connectAckId(const MockSocket::SentPacket& sent) {
  network::PacketStreamReader reader(sent.data);
  network::PacketHeader header{};
  network::NetworkId playerId{};
  reader >> header >> playerId;
  return playerId;
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
  const auto playerId = connectAckId(*ack);

  socket.clearSent();
  network::PacketStreamWriter leavePayload;
  leavePayload << playerId;
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
  const auto playerId = connectAckId(*ack);

  socket.clearSent();
  for (int tick = 0; tick < 5; ++tick) manager.update(0.25f);
  assert(std::any_of(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == static_cast<network::PacketType>(0x12);
  }));

  socket.clearSent();
  network::PacketStreamWriter leavePayload;
  leavePayload << playerId;
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
  const auto playerId = connectAckId(*ack);

  network::PacketStreamWriter inputPayload;
  inputPayload << playerId << std::uint8_t{2} << std::uint8_t{0}
               << std::uint8_t{0} << std::uint8_t{0} << std::uint8_t{0};
  socket.clearSent();
  socket.enqueue(packet(network::PacketType::PlayerInput,
                        inputPayload.getBuffer()), client);
  manager.update(0.016f);
  auto statePacket = std::find_if(socket.sent.begin(), socket.sent.end(), [](const auto& sent) {
    return packetType(sent) == network::PacketType::GameStateSync;
  });
  assert(statePacket != socket.sent.end());
  network::PacketStreamReader reader(statePacket->data);
  network::PacketHeader header{};
  std::uint16_t count{};
  network::EntityStateData state{};
  reader >> header >> count >> state;
  assert(count == 1);
  assert(state.netId == playerId);
  assert(state.velX == 0.0f && state.velY == 0.0f);
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
}

static void testEmptyRoomCleanup() {
  MockSocket socket;
  server::ServerNetworkManager manager(socket);
  const network::Endpoint owner{"127.0.0.1", 5300};
  socket.enqueue(packet(network::PacketType::RoomCreateReq,
                        payloadRoomCreate(2, 1)), owner);
  manager.update(0.016f);
  assert(manager.roomCount() == 2);
  for (int tick = 0; tick < 120; ++tick) manager.update(0.25f);
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
  testEmptyRoomCleanup();
  testReceiveThreadDoesNotOwnSimulationState();
  testUnknownRoomIsRejected();
  std::cout << "server room tests passed\n";
  return 0;
}
