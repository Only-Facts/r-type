#include "game/AnimationSystem.hpp"

namespace rtype::client {
  void AnimationSystem::update(engine::Registry& registry, float deltaTime) {
    registry.view<engine::AnimationComponent>([&](engine::Entity entity, engine::AnimationComponent& animation) {
      auto* sprite = registry.getComponent<engine::SpriteComponent>(entity);
      if (!sprite || !sprite->sprite || animation.frameDuration <= 0.0f) return;

      animation.elapsed += deltaTime;
      while (animation.elapsed >= animation.frameDuration) {
        animation.elapsed -= animation.frameDuration;
        const int cycleLength = getCycleLength(animation);
        if (!animation.loop && animation.step + 1 >= cycleLength) break;
        animation.step = (animation.step + 1) % cycleLength;
      }

      const int column = animation.firstColumn + getCurrentFrame(animation);
      sprite->sprite->setTextureRect(engine::IntRect{
        column * animation.frameWidth, animation.row * animation.frameHeight,
        animation.frameWidth, animation.frameHeight
      });
    });
  }

  int AnimationSystem::getCycleLength(const engine::AnimationComponent& animation) const {
    if (animation.frameCount <= 1) return 1;
    return animation.pingPong ? 2 * (animation.frameCount - 1) : animation.frameCount;
  }

  int AnimationSystem::getCurrentFrame(const engine::AnimationComponent& animation) const {
    if (animation.step < animation.frameCount) return animation.step;
    return getCycleLength(animation) - animation.step;
  }
}
