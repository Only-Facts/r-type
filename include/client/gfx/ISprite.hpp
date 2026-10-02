#ifndef ISPRITE_HPP
#define ISPRITE_HPP

#include "ITexture.hpp"
#include "GfxTypes.hpp"
#include <memory>

namespace rtype::engine {
  class ISprite {
  public:
    virtual ~ISprite() = default;

    virtual void setTexture(std::shared_ptr<ITexture> texture) = 0;
    virtual void setPosition(const Vector2f& position) = 0;
    virtual Vector2f getPosition() const = 0;

    virtual void setRotation(float angle) = 0;
    virtual float getRotation() const = 0;

    virtual void setScale(const Vector2f& scale) = 0;
    virtual Vector2f getScale() const = 0;

    virtual void setTextureRect(const IntRect& rect) = 0;
    virtual IntRect getTextureRect() const = 0;

    virtual void setColor(const Color& color) = 0;
  };
}

#endif // !ISPRITE_HPP
