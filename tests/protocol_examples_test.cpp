#include "ServerNetworkManager.hpp"
#include "engine/ecs/Registry.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/PacketStream.hpp"
#include "network/SnapshotDelta.hpp"
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace rtype;

namespace {
  class MockSocket final : public network::INetworkSocket {
  public:
    struct Datagram {
      network::PacketData data;
      network::Endpoint endpoint;
    };

    explicit MockSocket(unsigned short port) : _port(port) {}

    void bind(unsigned short = 0) override {}
    void setNonBlocking(bool) override {}
    unsigned short getLocalPort() const override { return _port; }

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

    std::vector<Datagram> takeSent() { return std::exchange(sent, {}); }

    std::deque<Datagram> incoming;
    std::vector<Datagram> sent;

  private:
    unsigned short _port;
  };

  const network::Endpoint SERVER{"127.0.0.1", 4242};
  const network::Endpoint CLIENT{"127.0.0.1", 50000};
  const network::Endpoint LOBBY_CLIENT{"127.0.0.1", 50001};

  struct Example {
    std::string name;
    network::PacketData actual;
    std::string expected;
  };

  std::string toHex(const network::PacketData& data) {
    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < data.size(); ++i) {
      if (i != 0) result += ' ';
      result += DIGITS[data[i] >> 4];
      result += DIGITS[data[i] & 0x0F];
    }
    return result;
  }

  bool matches(const network::PacketData& data, std::string_view pattern) {
    const std::string actual = toHex(data);
    if (actual.size() != pattern.size()) return false;
    for (std::size_t i = 0; i < actual.size(); ++i) {
      if (pattern[i] == 'x') continue;
      if (actual[i] != pattern[i]) return false;
    }
    return true;
  }

  network::PacketType typeOf(const network::PacketData& data) {
    return static_cast<network::PacketType>(data.at(2));
  }

  network::PacketData firstOfType(const std::vector<MockSocket::Datagram>& sent,
                                  network::PacketType type) {
    for (const auto& datagram : sent)
      if (typeOf(datagram.data) == type) return datagram.data;
    std::fprintf(stderr, "missing packet of type 0x%02x\n",
                 static_cast<unsigned>(type));
    std::exit(1);
  }

  network::PacketData packet(network::PacketType type, std::uint32_t sequence,
                             const network::PacketData& payload) {
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

  void deliver(MockSocket& to, const std::vector<MockSocket::Datagram>& sent,
               const network::Endpoint& from) {
    for (const auto& datagram : sent) to.incoming.push_back({datagram.data, from});
  }

  std::vector<Example> buildExamples() {
    std::vector<Example> examples;
    MockSocket serverSocket(4242);
    MockSocket clientSocket(50000);
    server::ServerNetworkManager server(serverSocket);
    engine::Registry registry;
    client::ClientNetworkManager client(clientSocket, registry);

    serverSocket.incoming.push_back(
      {packet(network::PacketType::RoomListReq, 1, {}), LOBBY_CLIENT});
    server.update(0.0f);
    auto sent = serverSocket.takeSent();
    examples.push_back({"RoomListReq", packet(network::PacketType::RoomListReq, 1, {}), ""});
    examples.push_back({"RoomListAck", firstOfType(sent, network::PacketType::RoomListAck), ""});

    network::PacketStreamWriter create;
    create << std::uint8_t{2} << std::uint8_t{2};
    const auto createReq = packet(network::PacketType::RoomCreateReq, 2, create.getBuffer());
    serverSocket.incoming.push_back({createReq, LOBBY_CLIENT});
    server.update(0.0f);
    sent = serverSocket.takeSent();
    examples.push_back({"RoomCreateReq", createReq, ""});
    examples.push_back({"RoomCreateAck", firstOfType(sent, network::PacketType::RoomCreateAck), ""});

    network::PacketStreamWriter ping;
    ping << std::uint64_t{0x0000019A3F2B7C10ull};
    const auto pingReq = packet(network::PacketType::Ping, 3, ping.getBuffer());
    serverSocket.incoming.push_back({pingReq, LOBBY_CLIENT});
    server.update(0.0f);
    sent = serverSocket.takeSent();
    examples.push_back({"Ping", pingReq, ""});
    examples.push_back({"Pong", firstOfType(sent, network::PacketType::Pong), ""});

    client.connectToServer(SERVER.address, SERVER.port, 1, 0);
    sent = clientSocket.takeSent();
    examples.push_back({"ConnectReq", firstOfType(sent, network::PacketType::ConnectReq), ""});
    deliver(serverSocket, sent, CLIENT);
    server.update(0.0f);
    sent = serverSocket.takeSent();
    examples.push_back({"ConnectAck", firstOfType(sent, network::PacketType::ConnectAck), ""});
    examples.push_back({"SpawnPlayer", firstOfType(sent, network::PacketType::SpawnPlayer), ""});
    examples.push_back({"HealthUpdate", firstOfType(sent, network::PacketType::HealthUpdate), ""});
    examples.push_back({"ScoreUpdate", firstOfType(sent, network::PacketType::ScoreUpdate), ""});
    examples.push_back({"GameStateSyncPacked",
                        firstOfType(sent, network::PacketType::GameStateSyncPacked), ""});

    std::vector<MockSocket::Datagram> connectAck{
      {firstOfType(sent, network::PacketType::ConnectAck), SERVER}};
    deliver(clientSocket, connectAck, SERVER);
    client.update(0.0f);
    sent = clientSocket.takeSent();
    examples.push_back({"Ack", firstOfType(sent, network::PacketType::Ack), ""});

    client.sendInputs(false, false, false, true, false);
    sent = clientSocket.takeSent();
    examples.push_back({"PlayerInput", firstOfType(sent, network::PacketType::PlayerInput), ""});
    deliver(serverSocket, sent, CLIENT);
    server.update(1.0f / 60.0f);
    sent = serverSocket.takeSent();
    examples.push_back({"InputAck", firstOfType(sent, network::PacketType::InputAck), ""});

    const network::SnapshotState base{
      {1000, network::packEntityState({1000, 200.0f, 300.0f, 0.0f, 0.0f})}};
    const network::SnapshotState current{
      {1000, network::packEntityState({1000, 205.0f, 300.0f, 300.0f, 0.0f})}};
    network::PacketStreamWriter delta;
    network::writeSnapshotDelta(delta, network::makeSnapshotDelta(base, 0, current, 1));
    examples.push_back({"GameStateSyncDelta",
                        packet(network::PacketType::GameStateSyncDelta, 9, delta.getBuffer()), ""});

    client.disconnect();
    sent = clientSocket.takeSent();
    examples.push_back({"Disconnect", firstOfType(sent, network::PacketType::Disconnect), ""});
    return examples;
  }
}

static const std::vector<std::pair<std::string, std::string>> EXPECTED{
  {"RoomListReq", "54 52 40 01 00 00 00 00 00"},
  {"RoomListAck", "54 52 41 01 00 00 00 0a 00 01 00 01 00 00 00 00 04 00 01"},
  {"RoomCreateReq", "54 52 42 02 00 00 00 02 00 02 02"},
  {"RoomCreateAck", "54 52 43 02 00 00 00 05 00 02 00 00 00 00"},
  {"Ping", "54 52 e0 03 00 00 00 08 00 10 7c 2b 3f 9a 01 00 00"},
  {"Pong", "54 52 e1 03 00 00 00 08 00 10 7c 2b 3f 9a 01 00 00"},
  {"ConnectReq", "54 52 01 01 00 00 00 02 00 01 00"},
  {"ConnectAck", "54 52 02 04 00 00 00 0d 00 e8 03 00 00 01 xx xx xx xx xx xx xx xx"},
  {"SpawnPlayer", "54 52 10 05 00 00 00 0d 00 e8 03 00 00 00 00 48 43 00 00 96 43 00"},
  {"HealthUpdate", "54 52 13 06 00 00 00 06 00 e8 03 00 00 05 05"},
  {"ScoreUpdate", "54 52 15 09 00 00 00 08 00 e8 03 00 00 00 00 00 00"},
  {"GameStateSyncPacked",
   "54 52 31 0b 00 00 00 12 00 00 00 00 00 01 00 e8 03 00 00 40 06 60 09 00 00 00 00"},
  {"Ack", "54 52 05 02 00 00 00 04 00 04 00 00 00"},
  {"PlayerInput",
   "54 52 20 03 00 00 00 11 00 e8 03 00 00 xx xx xx xx xx xx xx xx 00 00 00 01 00"},
  {"InputAck", "54 52 33 0c 00 00 00 10 00 03 00 00 00 01 00 00 00 00 00 4d 43 00 00 96 43"},
  {"GameStateSyncDelta",
   "54 52 32 09 00 00 00 15 00 01 00 00 00 00 00 00 00 01 00 e8 03 00 00 05 68 06 b0 04 00 00"},
  {"Disconnect", "54 52 03 04 00 00 00 0c 00 e8 03 00 00 xx xx xx xx xx xx xx xx"},
};

int main(int argc, char** argv) {
  const bool print = argc > 1 && std::string_view(argv[1]) == "--print";
  auto examples = buildExamples();
  if (!print && examples.size() != EXPECTED.size()) {
    std::fprintf(stderr, "expected %zu examples, built %zu\n",
                 EXPECTED.size(), examples.size());
    return 1;
  }
  for (std::size_t i = 0; i < examples.size() && !print; ++i) {
    if (examples[i].name != EXPECTED[i].first) {
      std::fprintf(stderr, "example %zu is %s, expected %s\n", i,
                   examples[i].name.c_str(), EXPECTED[i].first.c_str());
      return 1;
    }
    examples[i].expected = EXPECTED[i].second;
  }
  int failures = 0;
  for (const auto& example : examples) {
    if (print) {
      std::printf("%-20s %s\n", example.name.c_str(), toHex(example.actual).c_str());
      continue;
    }
    if (!matches(example.actual, example.expected)) {
      std::fprintf(stderr, "%s\n  expected: %s\n  actual:   %s\n",
                   example.name.c_str(), example.expected.c_str(),
                   toHex(example.actual).c_str());
      ++failures;
    }
  }
  return failures == 0 ? 0 : 1;
}
