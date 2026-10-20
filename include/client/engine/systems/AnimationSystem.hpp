#ifndef RTYPE_ENGINE_ANIMATIONSYSTEM_HPP
#define RTYPE_ENGINE_ANIMATIONSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"

namespace rtype::engine {
  class AnimationSystem : public ISystem {
  public:
    void update(Registry& registry, float dt) override {
      registry.view<AnimationComponent>([dt](Entity, AnimationComponent& anim) {
        if (anim.finished || anim.frameCount <= 1 || anim.frameTime <= 0.0f) return;

        anim.elapsed += dt;
        while (anim.elapsed >= anim.frameTime) {
          anim.elapsed -= anim.frameTime;
          if (anim.currentFrame + 1 < anim.frameCount) {
            ++anim.currentFrame;
          } else if (anim.loop) {
            anim.currentFrame = 0;
          } else {
            anim.finished = true;
            break;
          }
        }
      });
    }
  };
}

#endif // !RTYPE_ENGINE_ANIMATIONSYSTEM_HPP
