#ifndef RTYPE_GAMEPLAY_PLAYERMOVEMENT_HPP
#define RTYPE_GAMEPLAY_PLAYERMOVEMENT_HPP

#include <algorithm>
#include <cmath>

namespace rtype::gameplay {
  struct MovementInput {
    bool up{false};
    bool down{false};
    bool left{false};
    bool right{false};

    friend bool operator==(const MovementInput&, const MovementInput&) = default;
  };

  struct MovementVector {
    float x{0.0f};
    float y{0.0f};
  };

  struct MovementBounds {
    float worldWidth{1920.0f};
    float worldHeight{1080.0f};
    float playerWidth{33.0f};
    float playerHeight{34.0f};
  };

  inline constexpr float PLAYER_SPEED = 300.0f;
  inline constexpr float INPUT_STEP_SECONDS = 1.0f / 60.0f;

  inline MovementVector movementVelocity(const MovementInput& input, float speed = PLAYER_SPEED) {
    MovementVector velocity;
    if (input.up) velocity.y -= speed;
    if (input.down) velocity.y += speed;
    if (input.left) velocity.x -= speed;
    if (input.right) velocity.x += speed;
    const float magnitude = std::hypot(velocity.x, velocity.y);
    if (magnitude > speed) {
      velocity.x *= speed / magnitude;
      velocity.y *= speed / magnitude;
    }
    return velocity;
  }

  inline MovementVector applyMovement(MovementVector position, const MovementInput& input, float dt,
                                      const MovementBounds& bounds = {}, float speed = PLAYER_SPEED) {
    const MovementVector velocity = movementVelocity(input, speed);
    position.x = std::clamp(position.x + velocity.x * dt, 0.0f, std::max(0.0f, bounds.worldWidth - bounds.playerWidth));
    position.y = std::clamp(position.y + velocity.y * dt, 0.0f, std::max(0.0f, bounds.worldHeight - bounds.playerHeight));
    return position;
  }
}

#endif // !RTYPE_GAMEPLAY_PLAYERMOVEMENT_HPP
