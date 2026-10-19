#ifndef RTYPE_ENGINE_FLASHSYSTEM_HPP
#define RTYPE_ENGINE_FLASHSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"

namespace rtype::engine {
  class FlashSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.each<FlashComponent, SpriteComponent>([dt](Entity, FlashComponent& flash, SpriteComponent& sprite) {
        if (flash.remaining > 0.0f) {
          if (!flash.active) {
            flash.savedTint = sprite.tint;
            flash.active = true;
          }
          sprite.tint = flash.color;
          flash.remaining -= dt;
        } else if (flash.active) {
          sprite.tint = flash.savedTint;
          flash.active = false;
        }
      });
    }
  };
}

#endif // !RTYPE_ENGINE_FLASHSYSTEM_HPP
