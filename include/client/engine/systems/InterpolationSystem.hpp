#ifndef RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP
#define RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"

namespace rtype::engine {
  /// Moves the transform toward `targetPosition`. The network layer only writes
  /// targets; smoothing is a rendering concern, so it lives in the engine.
  class InterpolationSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.each<TransformComponent, InterpolationComponent>(
        [dt](Entity, TransformComponent& transform, InterpolationComponent& interp) {
          float factor = interp.speed * dt;
          if (factor > 1.0f) factor = 1.0f;
          transform.position.x += (transform.targetPosition.x - transform.position.x) * factor;
          transform.position.y += (transform.targetPosition.y - transform.position.y) * factor;
        });
    }
  };
}

#endif // !RTYPE_ENGINE_INTERPOLATIONSYSTEM_HPP
