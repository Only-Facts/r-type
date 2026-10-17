#ifndef PLAYERSTATUSSYSTEM_HPP
#define PLAYERSTATUSSYSTEM_HPP

#include "ecs/ISystem.hpp"

namespace rtype::client {
  class PlayerStatusSystem : public engine::ISystem {
  public:
    ~PlayerStatusSystem() override = default;

    void update(engine::Registry& registry, float deltaTime) override;

  private:
    static constexpr float BLINK_PERIOD = 0.15f;

    float _blinkTimer{0.0f};
  };
}

#endif // !PLAYERSTATUSSYSTEM_HPP
