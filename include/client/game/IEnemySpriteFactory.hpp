#ifndef IENEMYSPRITEFACTORY_HPP
#define IENEMYSPRITEFACTORY_HPP

#include "GameConstants.hpp"
#include "ecs/Components.hpp"
#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include <memory>

namespace rtype::client {
  class IEnemySpriteFactory {
  public:
    virtual ~IEnemySpriteFactory() = default;

    virtual std::shared_ptr<engine::ISprite> createSprite(game::EnemyType type) = 0;
    virtual engine::AnimationComponent createAnimation(game::EnemyType type) const = 0;
    virtual engine::Vector2f getSpriteScale() const = 0;
  };
}

#endif // !IENEMYSPRITEFACTORY_HPP
