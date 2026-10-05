#include "network/SFMLUdpSocket.hpp"
#include "exception.hpp"
#include "cli.hpp"
#include <SFML/Network/Dns.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <optional>

namespace rtype::network {
  void SFMLUdpSocket::bind(unsigned short port) {
    if (_socket.bind(port) != sf::Socket::Status::Done)
      throw RtypeError("Failed to bind UDP socket to port " + std::to_string(port));
  }

  void SFMLUdpSocket::setNonBlocking(bool nonBlocking) {
    _socket.setBlocking(!nonBlocking);
  }

  void SFMLUdpSocket::send(const PacketData& data, const Endpoint& endpoint) {
    auto addresses = sf::Dns::resolve(endpoint.address);

    if (!addresses || addresses->empty()) {
      error(("Failed to resolve address: " + std::string(endpoint.address)).c_str());
      return;
    }

    (void)_socket.send(data.data(), data.size(), addresses->front(), endpoint.port);
  }

  bool SFMLUdpSocket::receive(PacketData& data, Endpoint& outEndpoint) {
    std::size_t received = 0;
    std::optional<sf::IpAddress> remoteAddress;
    unsigned short remotePort = 0;

    sf::Socket::Status status = _socket.receive(_buffer.data(), _buffer.size(), received, remoteAddress, remotePort);

    if (status == sf::Socket::Status::Done && remoteAddress) {
      outEndpoint.address = remoteAddress->toString();
      outEndpoint.port = remotePort;

      data.assign(_buffer.begin(), _buffer.begin() + received);
      return true;
    }

    return false;
  }

  unsigned short SFMLUdpSocket::getLocalPort() const {
    return _socket.getLocalPort();
  }
}
