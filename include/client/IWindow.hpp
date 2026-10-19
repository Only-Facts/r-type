#ifndef IWINDOW_HPP
#define IWINDOW_HPP

#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include "input/IInputManager.hpp"

namespace rtype::engine {
  class IWindow {
  public:
    virtual ~IWindow() = default;

    virtual bool isOpen() const = 0;
    virtual void close() = 0;
    virtual void pollEvents(IInputManager& inputManager) = 0;
    virtual float getDeltaTime() = 0;

    virtual void clear(const Color& color = Color{0, 0, 0, 255}) = 0;
    virtual void draw(const ISprite& sprite) = 0;
    virtual void display() = 0;
  };
}

#endif // !IWINDOW_HPP
