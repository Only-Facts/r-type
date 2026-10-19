#ifndef RTYPE_ENGINE_IINPUTMANAGER_HPP
#define RTYPE_ENGINE_IINPUTMANAGER_HPP

#include "engine/input/KeyCodes.hpp"

namespace rtype::engine {
  class IInputManager {
  public:
    virtual ~IInputManager() = default;

    virtual void update() = 0;

    virtual bool isKeyPressed(KeyCode key) const = 0;
    virtual bool isKeyJustPressed(KeyCode key) const = 0;
    virtual bool isKeyJustReleased(KeyCode key) const = 0;
  };
}

#endif // !RTYPE_ENGINE_IINPUTMANAGER_HPP
