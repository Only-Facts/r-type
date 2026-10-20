#ifndef RTYPE_ENGINE_FOLLOWSYSTEM_HPP
#define RTYPE_ENGINE_FOLLOWSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include <vector>

namespace rtype::engine {
  class FollowSystem : public ISystem {
  public:
    void update(Registry& registry, float) override {
      _orphans.clear();
      registry.each<FollowComponent, TransformComponent>(
        [this, &registry](Entity entity, FollowComponent& follow, TransformComponent& transform) {
          const auto* target = registry.getComponent<TransformComponent>(follow.target);
          if (!target) {
            _orphans.push_back(entity);
            return;
          }
          transform.position = {target->position.x + follow.offset.x, target->position.y + follow.offset.y};
        });
      for (Entity entity : _orphans) registry.destroyEntity(entity);
    }

  private:
    std::vector<Entity> _orphans;
  };
}

#endif // !RTYPE_ENGINE_FOLLOWSYSTEM_HPP
