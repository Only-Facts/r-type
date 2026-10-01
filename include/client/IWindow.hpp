#ifndef IWINDOW_HPP
#define IWINDOW_HPP

#include "input/IInputManager.hpp"

namespace rtype::engine {
  class IWindow {
  public:
    virtual ~IWindow() = default;

    virtual bool isOpen() const = 0;
    virtual void close() = 0;
    virtual void clear() = 0;
    virtual void display() = 0;

    virtual void pollEvents(IInputManager& inputManager) = 0;
    virtual float getDeltaTime() = 0; 
  };
}

#endif // !IWINDOW_HPP
