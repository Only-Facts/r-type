#ifndef RTYPE_ENGINE_BLINKSYSTEM_HPP
#define RTYPE_ENGINE_BLINKSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include <cmath>

namespace rtype::engine {
  class BlinkSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.view<BlinkComponent>([&registry, dt](Entity entity, BlinkComponent& blink) {
        if (blink.period <= 0.0f) return;
        blink.elapsed = std::fmod(blink.elapsed + dt, blink.period);
        const bool visible = blink.elapsed < blink.period * blink.visibleRatio;

        if (auto* text = registry.getComponent<TextComponent>(entity)) text->visible = visible;
        if (auto* sprite = registry.getComponent<SpriteComponent>(entity)) sprite->visible = visible;
      });
    }
  };
}

#endif // !RTYPE_ENGINE_BLINKSYSTEM_HPP
