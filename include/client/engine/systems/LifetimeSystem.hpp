#ifndef RTYPE_ENGINE_LIFETIMESYSTEM_HPP
#define RTYPE_ENGINE_LIFETIMESYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include <vector>

namespace rtype::engine {
  /// Destroys entities whose LifetimeComponent runs out.
  class LifetimeSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      _expired.clear();
      registry.view<LifetimeComponent>([this, dt](Entity entity, LifetimeComponent& lifetime) {
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0.0f) _expired.push_back(entity);
      });
      // Destroy after the loop: erasing while iterating a pool is not allowed.
      for (Entity entity : _expired) registry.destroyEntity(entity);
    }

  private:
    std::vector<Entity> _expired;
  };
}

#endif // !RTYPE_ENGINE_LIFETIMESYSTEM_HPP
