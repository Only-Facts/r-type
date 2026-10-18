#ifndef RTYPE_ENGINE_IINPUTMANAGER_HPP
#define RTYPE_ENGINE_IINPUTMANAGER_HPP

#include "engine/input/KeyCodes.hpp"

namespace rtype::engine {
  /// Raw keyboard state, implemented by the backend.
  /// Games should prefer ActionMap, which works with action names instead of keys.
  class IInputManager {
  public:
    virtual ~IInputManager() = default;

    /// Called once per frame by the engine, before events are polled.
    virtual void update() = 0;

    virtual bool isKeyPressed(KeyCode key) const = 0;
    virtual bool isKeyJustPressed(KeyCode key) const = 0;
    virtual bool isKeyJustReleased(KeyCode key) const = 0;
  };
}

#endif // !RTYPE_ENGINE_IINPUTMANAGER_HPP
