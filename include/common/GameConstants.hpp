#ifndef GAMECONSTANTS_HPP
#define GAMECONSTANTS_HPP

#include "Hitbox.hpp"
#include <cstddef>
#include <cstdint>

namespace rtype::game {
  inline constexpr float WORLD_WIDTH = 1920.0f;
  inline constexpr float WORLD_HEIGHT = 1080.0f;

  inline constexpr float WORLD_DESPAWN_MARGIN = 100.0f;

  inline constexpr std::uint32_t FIRST_PROJECTILE_ID = 1'000'000;

  inline constexpr float PLAYER_SPEED = 500.0f;
  inline constexpr float PLAYER_SHOOT_COOLDOWN = 0.25f;
  inline constexpr float PLAYER_MUZZLE_OFFSET_X = 99.0f;
  inline constexpr float PLAYER_MUZZLE_OFFSET_Y = 8.0f;

  inline constexpr float PLAYER_WIDTH = 99.0f;
  inline constexpr float PLAYER_HEIGHT = 51.0f;
  inline constexpr Hitbox PLAYER_HITBOX{6.0f, 12.0f, 87.0f, 36.0f};

  inline constexpr std::uint8_t PLAYER_START_LIVES = 3;
  inline constexpr float PLAYER_INVINCIBILITY_DURATION = 2.0f;
  inline constexpr float PLAYER_EXPLOSION_SCALE = 5.0f;

  inline constexpr float MISSILE_SPEED = 900.0f;
  inline constexpr int MISSILE_DAMAGE = 1;
  inline constexpr Hitbox MISSILE_HITBOX{0.0f, 6.0f, 48.0f, 24.0f};

  enum class EnemyType : std::uint8_t {
    Spinner = 0,
    Count
  };

  inline constexpr std::uint32_t FIRST_ENEMY_ID = 2'000'000;
  inline constexpr std::size_t MAX_ENEMIES = 24;
  inline constexpr float ENEMY_SPAWN_MIN_DELAY = 0.6f;
  inline constexpr float ENEMY_SPAWN_MAX_DELAY = 1.8f;
}

#endif // !GAMECONSTANTS_HPP
