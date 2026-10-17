#ifndef IMISSILESPRITEFACTORY_HPP
#define IMISSILESPRITEFACTORY_HPP

#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include <memory>

namespace rtype::client {
  class IMissileSpriteFactory {
  public:
    virtual ~IMissileSpriteFactory() = default;

    virtual std::shared_ptr<engine::ISprite> createSprite() = 0;
    virtual engine::Vector2f getSpriteScale() const = 0;
  };
}

#endif // !IMISSILESPRITEFACTORY_HPP
