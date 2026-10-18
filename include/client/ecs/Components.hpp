#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include <memory>
#include <cstdint>

namespace rtype::engine {
  struct TransformComponent {
    Vector2f position{0.0f, 0.0f};
    float rotation{0.0f};
    Vector2f scale{1.0f, 1.0f};
    Vector2f targetPosition{0.0f, 0.0f};
  };

  struct VelocityComponent {
    Vector2f velocity{0.0f, 0.0f};
  };

  struct SpriteComponent {
    std::shared_ptr<ISprite> sprite{nullptr};
  };
  struct ProjectileComponent {
    std::uint32_t ownerId{0};
  };

  struct EnemyComponent {
    std::uint8_t type{0};
  };

  struct PlayerComponent {
    std::uint32_t score{0};
    std::uint8_t lives{0};
    bool isInvincible{false};
  };

  struct LifetimeComponent {
    float remaining{0.0f};
  };

  struct ShipTiltComponent {
    int frameWidth{0};
    int frameHeight{0};
    int row{0};
    int neutralColumn{0};
    int maxTilt{0};
    float tilt{0.0f};
  };

  struct AnimationComponent {
    int frameWidth{0};
    int frameHeight{0};
    int row{0};
    int firstColumn{0};
    int frameCount{1};
    float frameDuration{0.1f};
    bool pingPong{false};
    bool loop{true};
    float elapsed{0.0f};
    int step{0};
  };

  struct NetworkIdComponent {
    std::uint32_t netId{0};
    bool isLocalPlayer{false};
  };
}

#endif // !COMPONENTS_HPP
