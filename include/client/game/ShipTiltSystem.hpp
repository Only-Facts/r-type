#ifndef SHIPTILTSYSTEM_HPP
#define SHIPTILTSYSTEM_HPP

#include "ecs/ISystem.hpp"
#include "ecs/Components.hpp"

namespace rtype::client {
  class ShipTiltSystem : public engine::ISystem {
  public:
    ~ShipTiltSystem() override = default;

    void update(engine::Registry& registry, float deltaTime) override;

  private:
    static constexpr float TILT_SPEED = 8.0f;
    static constexpr float VERTICAL_SPEED_THRESHOLD = 1.0f;

    static float getTargetTilt(const engine::ShipTiltComponent& tilt, float verticalSpeed);
    static float moveTowards(float current, float target, float maxDelta);
  };
}

#endif // !SHIPTILTSYSTEM_HPP
