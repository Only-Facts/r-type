#include "SFMLWindow.hpp"
#include "input/SFMLInputManager.hpp"

namespace rtype::engine {
  SFMLWindow::SFMLWindow(unsigned int width, unsigned int height, const std::string& title)
    : _window(sf::VideoMode(sf::Vector2u(width, height)), title), _deltaTime(0.0f) {
    _window.setFramerateLimit(60);
  }

  bool SFMLWindow::isOpen() const {
    return _window.isOpen();
  }

  void SFMLWindow::close() {
    _window.close();
  }

  void SFMLWindow::clear() {
    _window.clear(sf::Color::Black);
  }

  void SFMLWindow::display() {
    _window.display();
  }

  void SFMLWindow::pollEvents(IInputManager& inputManager) {
    auto* sfmlInput = dynamic_cast<SFMLInputManager*>(&inputManager);

    while (auto event = _window.pollEvent()) {
      if (event->is<sf::Event::Closed>()) close();
      if (sfmlInput) sfmlInput->handleEvent(*event);
    }
  }

  float SFMLWindow::getDeltaTime() {
    _deltaTime = _clock.restart().asSeconds();
    return _deltaTime;
  }
}
