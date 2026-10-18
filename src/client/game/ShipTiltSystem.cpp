#include "game/ShipTiltSystem.hpp"
#include <algorithm>
#include <cmath>

namespace rtype::client {
  void ShipTiltSystem::update(engine::Registry& registry, float deltaTime) {
    registry.view<engine::ShipTiltComponent>([&](engine::Entity entity, engine::ShipTiltComponent& tilt) {
      auto* sprite = registry.getComponent<engine::SpriteComponent>(entity);
      if (!sprite || !sprite->sprite) return;

      auto* velocity = registry.getComponent<engine::VelocityComponent>(entity);
      const float verticalSpeed = velocity ? velocity->velocity.y : 0.0f;

      tilt.tilt = moveTowards(tilt.tilt, getTargetTilt(tilt, verticalSpeed), TILT_SPEED * deltaTime);

      const int column = tilt.neutralColumn + static_cast<int>(std::lround(tilt.tilt));
      sprite->sprite->setTextureRect(engine::IntRect{
        column * tilt.frameWidth, tilt.row * tilt.frameHeight, tilt.frameWidth, tilt.frameHeight
      });
    });
  }

  float ShipTiltSystem::getTargetTilt(const engine::ShipTiltComponent& tilt, float verticalSpeed) {
    if (verticalSpeed < -VERTICAL_SPEED_THRESHOLD) return static_cast<float>(tilt.maxTilt);
    if (verticalSpeed > VERTICAL_SPEED_THRESHOLD) return -static_cast<float>(tilt.maxTilt);
    return 0.0f;
  }

  float ShipTiltSystem::moveTowards(float current, float target, float maxDelta) {
    if (current < target) return std::min(current + maxDelta, target);
    return std::max(current - maxDelta, target);
  }
}
