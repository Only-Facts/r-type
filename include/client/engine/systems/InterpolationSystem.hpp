#ifndef RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP
#define RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"

namespace rtype::engine {
  class InterpolationSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.each<TransformComponent, InterpolationComponent>(
        [dt, &registry](Entity entity, TransformComponent& transform, InterpolationComponent& interp) {
          const auto* buffer = registry.getComponent<SnapshotBufferComponent>(entity);
          if (buffer && !buffer->samples.empty()) return;
          float factor = interp.speed * dt;
          if (factor > 1.0f) factor = 1.0f;
          transform.position.x += (transform.targetPosition.x - transform.position.x) * factor;
          transform.position.y += (transform.targetPosition.y - transform.position.y) * factor;
        });
    }
  };
}

#endif // !RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP
