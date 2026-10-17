#include "game/ProjectileSystem.hpp"
#include "GameConstants.hpp"
#include "ecs/Components.hpp"
#include <vector>

namespace rtype::client {
  void ProjectileSystem::update(engine::Registry& registry, float deltaTime) {
    std::vector<engine::Entity> expiredEntities;

    registry.view<engine::ProjectileComponent>([&](engine::Entity entity, engine::ProjectileComponent&) {
      auto* transform = registry.getComponent<engine::TransformComponent>(entity);
      auto* velocity = registry.getComponent<engine::VelocityComponent>(entity);
      if (!transform || !velocity) return;

      transform->position.x += velocity->velocity.x * deltaTime;
      transform->position.y += velocity->velocity.y * deltaTime;

      if (isOutsideWorld(transform->position)) expiredEntities.push_back(entity);
    });

    for (engine::Entity entity : expiredEntities)
      registry.destroyEntity(entity);
  }

  bool ProjectileSystem::isOutsideWorld(const engine::Vector2f& position) const {
    const float margin = game::WORLD_DESPAWN_MARGIN;
    return position.x < -margin || position.x > game::WORLD_WIDTH + margin
        || position.y < -margin || position.y > game::WORLD_HEIGHT + margin;
  }
}
