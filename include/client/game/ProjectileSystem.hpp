#ifndef PROJECTILESYSTEM_HPP
#define PROJECTILESYSTEM_HPP

#include "ecs/ISystem.hpp"
#include "gfx/GfxTypes.hpp"

namespace rtype::client {
  class ProjectileSystem : public engine::ISystem {
  public:
    ~ProjectileSystem() override = default;

    void update(engine::Registry& registry, float deltaTime) override;

  private:
    bool isOutsideWorld(const engine::Vector2f& position) const;
  };
}

#endif // !PROJECTILESYSTEM_HPP
