#include "backend/sfml/SFMLBackend.hpp"
#include "network/SFMLUdpSocket.hpp"

namespace rtype::backend {
  SFMLBackend::SFMLBackend(const engine::WindowConfig& config) : _window(config) {}

  void SFMLBackend::pollEvents() {
    sf::RenderWindow& native = _window.getNative();

    while (const auto event = native.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        _window.close();
      } else if (event->is<sf::Event::FocusLost>()) {
        _input.releaseAll();
      }
      _input.handleEvent(*event);
    }
  }

  std::unique_ptr<network::INetworkSocket> SFMLBackend::createUdpSocket() {
    return std::make_unique<network::SFMLUdpSocket>();
  }
}
