#ifndef RTYPE_ENGINE_ITEXT_HPP
#define RTYPE_ENGINE_ITEXT_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <string>

namespace rtype::engine {
  /// Backend-agnostic drawable text.
  class IText {
  public:
    virtual ~IText() = default;

    virtual void setString(const std::string& text) = 0;
    virtual void setCharacterSize(unsigned int size) = 0;
    virtual void setPosition(const Vector2f& position) = 0;
    virtual void setColor(const Color& color) = 0;
    /// Outline around the glyphs, for readability over busy backgrounds (0 = none).
    virtual void setOutline(const Color& color, float thickness) = 0;
    /// Size of the rendered text in pixels, used for alignment.
    virtual Vector2f getSize() const = 0;
  };
}

#endif // !RTYPE_ENGINE_ITEXT_HPP
