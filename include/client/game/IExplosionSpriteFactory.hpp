#ifndef IEXPLOSIONSPRITEFACTORY_HPP
#define IEXPLOSIONSPRITEFACTORY_HPP

#include "ecs/Components.hpp"
#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include <memory>

namespace rtype::client {
  class IExplosionSpriteFactory {
  public:
    virtual ~IExplosionSpriteFactory() = default;

    virtual std::shared_ptr<engine::ISprite> createSprite() = 0;
    virtual engine::AnimationComponent createAnimation() const = 0;
    virtual engine::Vector2f getFrameSize() const = 0;
    virtual float getDuration() const = 0;
  };
}

#endif // !IEXPLOSIONSPRITEFACTORY_HPP
