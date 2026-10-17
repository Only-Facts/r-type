#include "game/LifetimeSystem.hpp"
#include "ecs/Components.hpp"
#include <vector>

namespace rtype::client {
  void LifetimeSystem::update(engine::Registry& registry, float deltaTime) {
    std::vector<engine::Entity> expiredEntities;

    registry.view<engine::LifetimeComponent>([&](engine::Entity entity, engine::LifetimeComponent& lifetime) {
      lifetime.remaining -= deltaTime;
      if (lifetime.remaining <= 0.0f) expiredEntities.push_back(entity);
    });

    for (engine::Entity entity : expiredEntities)
      registry.destroyEntity(entity);
  }
}
