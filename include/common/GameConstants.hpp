#ifndef GAMECONSTANTS_HPP
#define GAMECONSTANTS_HPP

#include <cstdint>

namespace rtype::game {
  inline constexpr float WORLD_WIDTH = 1920.0f;
  inline constexpr float WORLD_HEIGHT = 1080.0f;

  inline constexpr float WORLD_DESPAWN_MARGIN = 100.0f;

  inline constexpr std::uint32_t FIRST_PROJECTILE_ID = 1'000'000;

  inline constexpr float PLAYER_SHOOT_COOLDOWN = 0.25f;
  inline constexpr float PLAYER_MUZZLE_OFFSET_X = 99.0f;
  inline constexpr float PLAYER_MUZZLE_OFFSET_Y = 8.0f;

  inline constexpr float MISSILE_SPEED = 900.0f;
}

#endif // !GAMECONSTANTS_HPP
