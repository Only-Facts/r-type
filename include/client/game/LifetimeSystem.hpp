#ifndef LIFETIMESYSTEM_HPP
#define LIFETIMESYSTEM_HPP

#include "ecs/ISystem.hpp"

namespace rtype::client {
  class LifetimeSystem : public engine::ISystem {
  public:
    ~LifetimeSystem() override = default;

    void update(engine::Registry& registry, float deltaTime) override;
  };
}

#endif // !LIFETIMESYSTEM_HPP
