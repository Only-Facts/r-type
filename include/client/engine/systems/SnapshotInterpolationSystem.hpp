#ifndef RTYPE_ENGINE_SNAPSHOTINTERPOLATIONSYSTEM_HPP
#define RTYPE_ENGINE_SNAPSHOTINTERPOLATIONSYSTEM_HPP

#include "engine/core/SnapshotClock.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/ecs/ISystem.hpp"
#include "engine/ecs/SnapshotBuffer.hpp"

namespace rtype::engine {
  class SnapshotInterpolationSystem : public ISystem {
  public:
    explicit SnapshotInterpolationSystem(SnapshotClock& clock) : _clock(clock) {}

    void update(Registry& registry, float) override {
      if (!_clock.isSynchronized()) return;
      const double renderTick = _clock.nextRenderTick();
      const double tickRate = _clock.tickRate();
      const double maxExtrapolation = _clock.maxExtrapolationTicks();
      registry.each<TransformComponent, SnapshotBufferComponent>(
        [&](Entity, TransformComponent& transform, SnapshotBufferComponent& buffer) {
          const auto position = sampleSnapshotPosition(buffer, renderTick, tickRate, maxExtrapolation);
          if (!position) return;
          transform.position = *position;
          transform.targetPosition = *position;
        });
    }

  private:
    SnapshotClock& _clock;
  };
}

#endif // !RTYPE_ENGINE_SNAPSHOTINTERPOLATIONSYSTEM_HPP
