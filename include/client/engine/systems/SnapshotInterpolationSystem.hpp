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
      const bool synchronized = _clock.isSynchronized();
      const double renderTick = synchronized ? _clock.nextRenderTick() : 0.0;
      const double tickRate = _clock.tickRate();
      const double maxExtrapolation = _clock.maxExtrapolationTicks();

      registry.each<TransformComponent, SnapshotBufferComponent>(
        [&](Entity entity, TransformComponent& transform, SnapshotBufferComponent& buffer) {
          if (!buffer.revealed) {
            const bool reached = synchronized && !buffer.samples.empty() &&
                                 renderTick >= static_cast<double>(buffer.samples.front().tick);
            if (!reached) {
              setVisible(registry, entity, false);
              return;
            }
            buffer.revealed = true;
            setVisible(registry, entity, true);
          }
          if (!synchronized) return;
          const auto position = sampleSnapshotPosition(buffer, renderTick, tickRate, maxExtrapolation);
          if (!position) return;
          transform.position = *position;
          transform.targetPosition = *position;
        });

      registry.each<FollowComponent>([&registry](Entity entity, FollowComponent& follow) {
        const auto* target = registry.getComponent<SnapshotBufferComponent>(follow.target);
        if (target) setVisible(registry, entity, target->revealed);
      });
    }

  private:
    static void setVisible(Registry& registry, Entity entity, bool visible) {
      if (auto* sprite = registry.getComponent<SpriteComponent>(entity)) sprite->visible = visible;
      if (auto* text = registry.getComponent<TextComponent>(entity)) text->visible = visible;
    }

    SnapshotClock& _clock;
  };
}

#endif // !RTYPE_ENGINE_SNAPSHOTINTERPOLATIONSYSTEM_HPP
