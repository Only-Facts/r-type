#ifndef RTYPE_ENGINE_IWINDOW_HPP
#define RTYPE_ENGINE_IWINDOW_HPP

#include "engine/gfx/GfxTypes.hpp"
#include "engine/gfx/ISprite.hpp"
#include "engine/gfx/IText.hpp"

namespace rtype::engine {
  /// Render target abstraction. Event polling and timing are no longer the window's
  /// job: events are pumped by IBackend, time is measured by the engine Clock.
  class IWindow {
  public:
    virtual ~IWindow() = default;

    virtual bool isOpen() const = 0;
    virtual void close() = 0;
    virtual Vector2u getSize() const = 0;

    virtual void clear(const Color& color) = 0;
    virtual void draw(const ISprite& sprite) = 0;
    virtual void draw(const IText& text) = 0;
    virtual void display() = 0;
  };
}

#endif // !RTYPE_ENGINE_IWINDOW_HPP
