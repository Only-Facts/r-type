#ifndef RTYPE_ENGINE_LIFETIMESYSTEM_HPP
#define RTYPE_ENGINE_LIFETIMESYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace rtype::engine {
  class LifetimeSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      _expired.clear();
      registry.view<LifetimeComponent>([this, &registry, dt](Entity entity, LifetimeComponent& lifetime) {
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0.0f) {
          _expired.push_back(entity);
          return;
        }
        if (lifetime.fadeOut && lifetime.duration > 0.0f && lifetime.fadePortion > 0.0f)
          fade(registry, entity, lifetime.remaining / (lifetime.duration * lifetime.fadePortion));
      });
      for (Entity entity : _expired) registry.destroyEntity(entity);
    }

  private:
    static void fade(Registry& registry, Entity entity, float ratio) {
      const auto alpha = static_cast<std::uint8_t>(std::clamp(ratio, 0.0f, 1.0f) * 255.0f);
      if (auto* text = registry.getComponent<TextComponent>(entity)) {
        text->color.a = alpha;
        text->outlineColor.a = alpha;
      }
      if (auto* sprite = registry.getComponent<SpriteComponent>(entity)) {
        Color tint = sprite->tint.value_or(Color{255, 255, 255, 255});
        tint.a = alpha;
        sprite->tint = tint;
      }
    }

    std::vector<Entity> _expired;
  };
}

#endif // !RTYPE_ENGINE_LIFETIMESYSTEM_HPP
