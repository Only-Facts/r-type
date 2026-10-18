#ifndef RTYPE_ENGINE_MOVEMENTSYSTEM_HPP
#define RTYPE_ENGINE_MOVEMENTSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"

namespace rtype::engine {
  /// position += velocity * dt (time-based, not frame-based).
  class MovementSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.each<TransformComponent, VelocityComponent>(
        [dt](Entity, TransformComponent& transform, VelocityComponent& velocity) {
          transform.position.x += velocity.velocity.x * dt;
          transform.position.y += velocity.velocity.y * dt;
        });
    }
  };
}

#endif // !RTYPE_ENGINE_MOVEMENTSYSTEM_HPP
