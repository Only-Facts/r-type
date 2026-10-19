#ifndef RTYPE_ENGINE_ITEXT_HPP
#define RTYPE_ENGINE_ITEXT_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <string>

namespace rtype::engine {
  class IText {
  public:
    virtual ~IText() = default;

    virtual void setString(const std::string& text) = 0;
    virtual void setCharacterSize(unsigned int size) = 0;
    virtual void setPosition(const Vector2f& position) = 0;
    virtual void setColor(const Color& color) = 0;
    virtual void setOutline(const Color& color, float thickness) = 0;
    virtual Vector2f getSize() const = 0;
  };
}

#endif // !RTYPE_ENGINE_ITEXT_HPP
