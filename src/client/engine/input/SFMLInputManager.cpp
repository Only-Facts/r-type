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
    _keyMap[sf::Keyboard::Key::Tab] = KeyCode::Tab;

    _keyMap[sf::Keyboard::Key::Z] = KeyCode::KeyZ;
    _keyMap[sf::Keyboard::Key::Q] = KeyCode::KeyQ;
    _keyMap[sf::Keyboard::Key::S] = KeyCode::KeyS;
    _keyMap[sf::Keyboard::Key::D] = KeyCode::KeyD;
  }

  void SFMLInputManager::update() {
    _previousStates = _currentStates;
    _textEntered.clear();
  }

  void SFMLInputManager::handleEvent(const sf::Event& event) {
    if (const auto* pressed = event.getIf<sf::Event::KeyPressed>())
      setKeyState(pressed->code, true);
    else if (const auto* released = event.getIf<sf::Event::KeyReleased>())
      setKeyState(released->code, false);
    else if (const auto* text = event.getIf<sf::Event::TextEntered>())
      appendTextEntered(text->unicode);
    else if (event.is<sf::Event::FocusLost>())
      _currentStates.fill(false);
  }

  void SFMLInputManager::appendTextEntered(char32_t character) {
    const bool isPrintableAscii = character >= U' ' && character <= U'~';
    if (isPrintableAscii || character == BACKSPACE_CHARACTER)
      _textEntered.push_back(static_cast<char>(character));
  }

  const std::string& SFMLInputManager::getTextEntered() const {
    return _textEntered;
  }

  void SFMLInputManager::setKeyState(sf::Keyboard::Key key, bool isPressed) {
    auto it = _keyMap.find(key);
    if (it != _keyMap.end()) _currentStates[static_cast<std::size_t>(it->second)] = isPressed;
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
