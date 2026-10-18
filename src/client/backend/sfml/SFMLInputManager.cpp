#include "backend/sfml/SFMLInputManager.hpp"

namespace rtype::backend {
  using engine::KeyCode;

  SFMLInputManager::SFMLInputManager() {
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
    _keyMap[sf::Keyboard::Key::Tab] = KeyCode::Tab;
    _keyMap[sf::Keyboard::Key::Backspace] = KeyCode::Backspace;

    // A..Z are contiguous in both enums.
    constexpr int letterCount = 26;
    for (int i = 0; i < letterCount; ++i) {
      const auto sfKey = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::A) + i);
      const auto code = static_cast<KeyCode>(static_cast<std::size_t>(KeyCode::KeyA) + static_cast<std::size_t>(i));
      _keyMap[sfKey] = code;
    }

    // F1..F12 are contiguous in both enums too.
    constexpr int functionKeyCount = 12;
    for (int i = 0; i < functionKeyCount; ++i) {
      const auto sfKey = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::F1) + i);
      const auto code = static_cast<KeyCode>(static_cast<std::size_t>(KeyCode::F1) + static_cast<std::size_t>(i));
      _keyMap[sfKey] = code;
    }
  }

  void SFMLInputManager::update() {
    // New frame: forget last frame's edges, keep the held state
    // (a held key stays down until its KeyReleased event).
    _pressedThisFrame.fill(false);
    _releasedThisFrame.fill(false);
  }

  void SFMLInputManager::handleEvent(const sf::Event& event) {
    if (const auto* pressed = event.getIf<sf::Event::KeyPressed>())
      setKey(pressed->code, true);
    else if (const auto* released = event.getIf<sf::Event::KeyReleased>())
      setKey(released->code, false);
  }

  void SFMLInputManager::releaseAll() {
    for (std::size_t i = 0; i < KEY_COUNT; ++i) {
      if (_down[i]) _releasedThisFrame[i] = true;
      _down[i] = false;
      _pressedThisFrame[i] = false;
    }
  }

  void SFMLInputManager::setKey(sf::Keyboard::Key key, bool pressed) {
    auto it = _keyMap.find(key);
    if (it == _keyMap.end()) return;

    const std::size_t index = indexOf(it->second);
    if (pressed && !_down[index]) _pressedThisFrame[index] = true;  // ignores OS key-repeat
    if (!pressed && _down[index]) _releasedThisFrame[index] = true;
    _down[index] = pressed;
  }

  std::size_t SFMLInputManager::indexOf(KeyCode key) {
    return static_cast<std::size_t>(key);
  }

  bool SFMLInputManager::isKeyPressed(KeyCode key) const {
    const std::size_t index = indexOf(key);
    // A tap shorter than a frame still counts as "pressed" for that frame.
    return index < KEY_COUNT && (_down[index] || _pressedThisFrame[index]);
  }

  bool SFMLInputManager::isKeyJustPressed(KeyCode key) const {
    const std::size_t index = indexOf(key);
    return index < KEY_COUNT && _pressedThisFrame[index];
  }

  bool SFMLInputManager::isKeyJustReleased(KeyCode key) const {
    const std::size_t index = indexOf(key);
    return index < KEY_COUNT && _releasedThisFrame[index];
  }
}
