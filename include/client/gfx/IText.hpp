#ifndef ITEXT_HPP
#define ITEXT_HPP

#include "GfxTypes.hpp"
#include <string>

namespace rtype::engine {
  class IText {
  public:
    virtual ~IText() = default;

    virtual void setString(const std::string& text) = 0;
    virtual void setPosition(const Vector2f& position) = 0;
    virtual void setColor(const Color& color) = 0;

    virtual Vector2f getSize() const = 0;
  };
}

#endif // !ITEXT_HPP
