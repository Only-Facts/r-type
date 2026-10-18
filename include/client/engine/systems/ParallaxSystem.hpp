#ifndef RTYPE_ENGINE_PARALLAXSYSTEM_HPP
#define RTYPE_ENGINE_PARALLAXSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include "engine/platform/IWindow.hpp"

namespace rtype::engine {
  class ParallaxSystem : public ISystem {
  public:
    explicit ParallaxSystem(const IWindow& window) : _window(window) {}

    void update(Registry& registry, float) override {
      const float width = static_cast<float>(_window.getSize().x);

      registry.each<TransformComponent, ParallaxComponent>(
        [width](Entity, TransformComponent& transform, ParallaxComponent& parallax) {
          const float span = width + 2.0f * parallax.wrapMargin;
          if (transform.position.x < -parallax.wrapMargin)
            transform.position.x += span;
          else if (transform.position.x > width + parallax.wrapMargin)
            transform.position.x -= span;
        });
    }

  private:
    const IWindow& _window;
  };
}

#endif // !RTYPE_ENGINE_PARALLAXSYSTEM_HPP
