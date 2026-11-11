#include "ServerNetworkManager.hpp"
#include "engine/ecs/Registry.hpp"
#include "network/ClientNetworkManager.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

using namespace rtype;

namespace {
  constexpr unsigned short SERVER_PORT = 4242;
  const network::Endpoint SERVER{"127.0.0.1", SERVER_PORT};
  const network::Endpoint CLIENT{"127.0.0.1", 50000};
  constexpr float FRAME = 1.0f / 60.0f;

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

  struct Link {
    std::function<bool(std::uint64_t)> dropToServer = [](std::uint64_t) { return false; };
    std::function<bool(std::uint64_t)> dropToClient = [](std::uint64_t) { return false; };
    std::uint64_t toServer{0};
    std::uint64_t toClient{0};
    std::uint64_t bytesToServer{0};
    std::uint64_t bytesToClient{0};
  };

  struct Session {
    Session() : server(serverSocket), client(clientSocket, registry) {}

    void frame(Link& link) {
      if (++frames % 30 == 0) client.sendInputs(false, false, false, false, false);
      client.update(FRAME);
      for (auto& [data, to] : clientSocket.sent) {
        if (link.dropToServer(++link.toServer)) continue;
        link.bytesToServer += data.size();
        serverSocket.incoming.emplace_back(data, CLIENT);
      }
      clientSocket.sent.clear();
      server.update(FRAME);
      for (auto& [data, to] : serverSocket.sent) {
        if (to.port != CLIENT.port || link.dropToClient(++link.toClient)) continue;
        link.bytesToClient += data.size();
        clientSocket.incoming.emplace_back(data, SERVER);
      }
      serverSocket.sent.clear();
    }

    [[nodiscard]] std::uint64_t inFlightToClient() const {
      std::uint64_t bytes = 0;
      for (const auto& [data, from] : clientSocket.incoming) bytes += data.size();
      return bytes;
    }

    [[nodiscard]] std::uint64_t packetsInFlightToClient() const { return clientSocket.incoming.size(); }

    void run(Link& link, float seconds) {
      const int count = static_cast<int>(std::lround(seconds / FRAME));
      for (int i = 0; i < count; ++i) frame(link);
    }

    MockSocket serverSocket;
    MockSocket clientSocket;
    server::ServerNetworkManager server;
    engine::Registry registry;
    client::ClientNetworkManager client;
    int frames{0};
  };

  void testMetricsMatchTheRealServer() {
    Session session;
    Link link;
    assert(session.client.connectToServer(SERVER.address, SERVER_PORT, 1, 0));
    session.run(link, 8.0f);
    assert(session.client.isConnected());

    const client::NetStats& stats = session.client.getStats();
    const server::ServerStats server = session.server.stats();
    assert(server.rateLimitedPackets == 0);

    assert(stats.bytesSent == server.receivedBytes);
    assert(stats.packetsSent == server.receivedDatagrams);
    assert(stats.bytesReceived + session.inFlightToClient() == link.bytesToClient);
    assert(stats.bytesReceived + session.inFlightToClient() == server.sentBytes);
    assert(stats.packetsReceived + session.packetsInFlightToClient() == server.sentDatagrams);
    assert(stats.bytesReceivedPerSecond > 0.0 && stats.bytesSentPerSecond > 0.0);

    assert(stats.pingsSent >= 15);
    assert(stats.pongsReceived + 1 >= stats.pingsSent);
    assert(stats.pingsLost == 0);
    assert(stats.rttMs && std::abs(*stats.rttMs - 1000.0 * FRAME) < 0.5);
    assert(stats.jitterMs && *stats.jitterMs < 0.5);
    assert(stats.lossPercent && *stats.lossPercent == 0.0);

    assert(stats.rejectedFor(client::RejectReason::WrongSender) == 0);
    assert(stats.rejectedFor(client::RejectReason::Malformed) == 0);
    assert(stats.retransmissionsSent == 0);
    assert(stats.retransmissionsReceived == 0);
    assert(server.reliableRetransmitted == 0);
  }

  void testMetricsSeeALossyLink() {
    Session session;
    Link link;
    link.dropToServer = [](std::uint64_t index) { return index > 2 && index % 5 == 0; };
    link.dropToClient = [](std::uint64_t index) { return index > 6 && index % 5 == 0; };
    assert(session.client.connectToServer(SERVER.address, SERVER_PORT, 1, 0));
    session.run(link, 8.0f);
    assert(session.client.isConnected());

    const client::NetStats& stats = session.client.getStats();
    const server::ServerStats server = session.server.stats();
    assert(server.rateLimitedPackets == 0);

    assert(stats.pingsLost > 0);
    assert(stats.lossPercent && *stats.lossPercent > 10.0 && *stats.lossPercent < 70.0);
    assert(stats.rttMs);

    assert(server.reliableRetransmitted > 0);
    assert(stats.retransmissionsReceived > 0);
    assert(stats.retransmissionsReceived <= server.reliableRetransmitted);
    assert(stats.rejectedFor(client::RejectReason::Duplicate) >= stats.retransmissionsReceived);

    assert(link.bytesToServer == server.receivedBytes);
    assert(stats.bytesSent > server.receivedBytes);
    assert(stats.bytesReceived + session.inFlightToClient() == link.bytesToClient);
    assert(stats.bytesReceived + session.inFlightToClient() < server.sentBytes);
  }
}

int main() {
  testMetricsMatchTheRealServer();
  testMetricsSeeALossyLink();
  return 0;
}
