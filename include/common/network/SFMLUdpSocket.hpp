#ifndef SFMLUDPSOCKET_HPP
#define SFMLUDPSOCKET_HPP

#include "INetworkSocket.hpp"
#include <SFML/Network/UdpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/Dns.hpp>
#include <array>

namespace rtype::network {
  class SFMLUdpSocket : public INetworkSocket {
  public:
    SFMLUdpSocket() = default;
    ~SFMLUdpSocket() override = default;

    void bind(unsigned short port) override;
    void setNonBlocking(bool nonBlocking) override;
    void send(const PacketData& data, const Endpoint& endpoint) override;
    bool receive(PacketData& data, Endpoint& outEndpoint) override;
    unsigned short getLocalPort() const override;

  private:
    sf::UdpSocket _socket;
    std::array<std::uint8_t, 65535> _buffer{};
  };
}

#endif // !SFMLUDPSOCKET_HPP
