#include "game/PlayerStatusSystem.hpp"
#include "ecs/Components.hpp"
#include <cmath>

namespace rtype::client {
  namespace {
    constexpr std::uint8_t VISIBLE_ALPHA = 255;
    constexpr std::uint8_t BLINK_ALPHA = 70;
    constexpr std::uint8_t HIDDEN_ALPHA = 0;
  }

  void PlayerStatusSystem::update(engine::Registry& registry, float deltaTime) {
    _blinkTimer = std::fmod(_blinkTimer + deltaTime, 2.0f * BLINK_PERIOD);
    const bool isBlinkOff = _blinkTimer >= BLINK_PERIOD;

    registry.view<engine::PlayerComponent>([&](engine::Entity entity, engine::PlayerComponent& player) {
      auto* sprite = registry.getComponent<engine::SpriteComponent>(entity);
      if (!sprite || !sprite->sprite) return;

      std::uint8_t alpha = VISIBLE_ALPHA;
      if (player.lives == 0) alpha = HIDDEN_ALPHA;
      else if (player.isInvincible && isBlinkOff) alpha = BLINK_ALPHA;

      sprite->sprite->setColor(engine::Color{255, 255, 255, alpha});
    });
  }
}
