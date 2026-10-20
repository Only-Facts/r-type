#ifndef RTYPE_ENGINE_SCREENANCHORSYSTEM_HPP
#define RTYPE_ENGINE_SCREENANCHORSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include "engine/platform/IWindow.hpp"

namespace rtype::engine {
  class ScreenAnchorSystem : public ISystem {
  public:
    explicit ScreenAnchorSystem(const IWindow& window) : _window(window) {}

    void update(Registry& registry, float) override {
      const Vector2u size = _window.getSize();
      const float width = static_cast<float>(size.x);
      const float height = static_cast<float>(size.y);

      registry.each<TransformComponent, ScreenAnchorComponent>(
        [width, height](Entity, TransformComponent& transform, ScreenAnchorComponent& anchor) {
          transform.position = {anchor.anchor.x * width + anchor.offset.x,
                                anchor.anchor.y * height + anchor.offset.y};
        });
    }

  private:
    const IWindow& _window;
  };
}

#endif // !RTYPE_ENGINE_SCREENANCHORSYSTEM_HPP
