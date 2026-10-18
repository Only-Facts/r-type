#include "backend/sfml/SFMLWindow.hpp"
#include "backend/sfml/SFMLSprite.hpp"
#include "backend/sfml/SFMLText.hpp"

namespace rtype::backend {
  SFMLWindow::SFMLWindow(const engine::WindowConfig& config)
    : _window(sf::VideoMode(sf::Vector2u(config.width, config.height)), config.title) {
    if (config.vsync) _window.setVerticalSyncEnabled(true);
    else _window.setFramerateLimit(config.framerateLimit);
  }

  bool SFMLWindow::isOpen() const {
    return _window.isOpen();
  }

  void SFMLWindow::close() {
    _window.close();
  }

  engine::Vector2u SFMLWindow::getSize() const {
    const sf::Vector2u size = _window.getSize();
    return engine::Vector2u{size.x, size.y};
  }

  void SFMLWindow::clear(const engine::Color& color) {
    _window.clear(sf::Color(color.r, color.g, color.b, color.a));
  }

  void SFMLWindow::draw(const engine::ISprite& sprite) {
    // Every sprite in this backend is an SFMLSprite (created by SFMLTextureManager).
    const auto* sfmlSprite = dynamic_cast<const SFMLSprite*>(&sprite);
    if (sfmlSprite && sfmlSprite->getNativeSprite()) _window.draw(*sfmlSprite->getNativeSprite());
  }

  void SFMLWindow::draw(const engine::IText& text) {
    const auto* sfmlText = dynamic_cast<const SFMLText*>(&text);
    if (sfmlText) _window.draw(sfmlText->getNative());
  }

  void SFMLWindow::display() {
    _window.display();
  }
}
