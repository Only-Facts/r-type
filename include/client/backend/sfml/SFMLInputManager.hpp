#ifndef RTYPE_BACKEND_SFMLINPUTMANAGER_HPP
#define RTYPE_BACKEND_SFMLINPUTMANAGER_HPP

#include "engine/input/IInputManager.hpp"
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <array>
#include <cstddef>
#include <unordered_map>

namespace rtype::backend {
  class SFMLInputManager : public engine::IInputManager {
  public:
    SFMLInputManager();
    ~SFMLInputManager() override = default;

    void update() override;

    bool isKeyPressed(engine::KeyCode key) const override;
    bool isKeyJustPressed(engine::KeyCode key) const override;
    bool isKeyJustReleased(engine::KeyCode key) const override;

    void handleEvent(const sf::Event& event);
    void releaseAll();

  private:
    static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(engine::KeyCode::Count);

    void initKeyMap();
    void setKey(sf::Keyboard::Key key, bool pressed);
    static std::size_t indexOf(engine::KeyCode key);

    std::array<bool, KEY_COUNT> _down{};
    std::array<bool, KEY_COUNT> _pressedThisFrame{};
    std::array<bool, KEY_COUNT> _releasedThisFrame{};
    std::unordered_map<sf::Keyboard::Key, engine::KeyCode> _keyMap;
  };
}

#endif // !RTYPE_BACKEND_SFMLINPUTMANAGER_HPP
