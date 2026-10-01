#ifndef SFMLINPUTMANAGER_HPP
#define SFMLINPUTMANAGER_HPP

#include "IInputManager.hpp"
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <array>
#include <unordered_map>

namespace rtype::engine {
  class SFMLInputManager : public IInputManager {
  public:
    SFMLInputManager();
    ~SFMLInputManager() override = default;

    void update() override;

    bool isKeyPressed(KeyCode key) const override;
    bool isKeyJustPressed(KeyCode key) const override;
    bool isKeyJustReleased(KeyCode key) const override;

    void handleEvent(const sf::Event& event);

  private:
    static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(KeyCode::Count);

    std::array<bool, KEY_COUNT> _currentStates{};
    std::array<bool, KEY_COUNT> _previousStates{};
    
    std::unordered_map<sf::Keyboard::Key, KeyCode> _keyMap;

    void initKeyMap();
  };
}

#endif // !SFMLINPUTMANAGER_HPP
