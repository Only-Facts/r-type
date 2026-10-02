#include "input/SFMLInputManager.hpp"

namespace rtype::engine {
  SFMLInputManager::SFMLInputManager() {
    _currentStates.fill(false);
    _previousStates.fill(false);
    initKeyMap();
  }

  void SFMLInputManager::initKeyMap() {
    _keyMap[sf::Keyboard::Key::Up] = KeyCode::Up;
    _keyMap[sf::Keyboard::Key::Down] = KeyCode::Down;
    _keyMap[sf::Keyboard::Key::Left] = KeyCode::Left;
    _keyMap[sf::Keyboard::Key::Right] = KeyCode::Right;
    _keyMap[sf::Keyboard::Key::Space] = KeyCode::Space;
    _keyMap[sf::Keyboard::Key::Escape] = KeyCode::Escape;
    _keyMap[sf::Keyboard::Key::Enter] = KeyCode::Enter;

    _keyMap[sf::Keyboard::Key::Z] = KeyCode::KeyZ;
    _keyMap[sf::Keyboard::Key::Q] = KeyCode::KeyQ;
    _keyMap[sf::Keyboard::Key::S] = KeyCode::KeyS;
    _keyMap[sf::Keyboard::Key::D] = KeyCode::KeyD;
  }

  void SFMLInputManager::update() {
    _previousStates = _currentStates;
  }

  void SFMLInputManager::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::KeyPressed>() || event.is<sf::Event::KeyReleased>()) {
      auto key = event.getIf<sf::Event::KeyPressed>();
      if (key != nullptr) {
        auto it = _keyMap.find(key->code);
        if (it != _keyMap.end()) _currentStates[static_cast<std::size_t>(it->second)] = (event.is<sf::Event::KeyPressed>());
      }
    }
  }

  bool SFMLInputManager::isKeyPressed(KeyCode key) const {
    std::size_t index = static_cast<std::size_t>(key);
    if (index >= KEY_COUNT) return false;
    return _currentStates[index];
  }

  bool SFMLInputManager::isKeyJustPressed(KeyCode key) const {
    std::size_t index = static_cast<std::size_t>(key);
    if (index >= KEY_COUNT) return false;
    return _currentStates[index] && !_previousStates[index];
  }

  bool SFMLInputManager::isKeyJustReleased(KeyCode key) const {
    std::size_t index = static_cast<std::size_t>(key);
    if (index >= KEY_COUNT) return false;
    return !_currentStates[index] && _previousStates[index];
  }
}
