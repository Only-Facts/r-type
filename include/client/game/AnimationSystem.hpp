#ifndef ANIMATIONSYSTEM_HPP
#define ANIMATIONSYSTEM_HPP

#include "ecs/ISystem.hpp"
#include "ecs/Components.hpp"

namespace rtype::client {
  class AnimationSystem : public engine::ISystem {
  public:
    ~AnimationSystem() override = default;

    void update(engine::Registry& registry, float deltaTime) override;

  private:
    int getCycleLength(const engine::AnimationComponent& animation) const;
    int getCurrentFrame(const engine::AnimationComponent& animation) const;
  };
}

#endif // !ANIMATIONSYSTEM_HPP
