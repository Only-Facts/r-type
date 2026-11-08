#ifndef RTYPE_ENGINE_SNAPSHOTBUFFER_HPP
#define RTYPE_ENGINE_SNAPSHOTBUFFER_HPP

#include "engine/ecs/Components.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype::engine {
  enum class SnapshotInsertResult { Inserted, Duplicate, TooOld };

  inline constexpr std::size_t MIN_SNAPSHOT_BUFFER_CAPACITY = 2;

  inline SnapshotInsertResult pushSnapshotSample(SnapshotBufferComponent& buffer, const SnapshotSample& sample) {
    auto& samples = buffer.samples;
    const std::size_t capacity = std::max(buffer.capacity, MIN_SNAPSHOT_BUFFER_CAPACITY);
    if (samples.size() >= capacity && sample.tick < samples.front().tick) return SnapshotInsertResult::TooOld;

    auto position = std::lower_bound(samples.begin(), samples.end(), sample.tick,
                                     [](const SnapshotSample& stored, std::uint32_t tick) { return stored.tick < tick; });
    if (position != samples.end() && position->tick == sample.tick) return SnapshotInsertResult::Duplicate;

    samples.insert(position, sample);
    if (samples.size() > capacity)
      samples.erase(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(samples.size() - capacity));
    return SnapshotInsertResult::Inserted;
  }

  inline std::optional<Vector2f> sampleSnapshotPosition(const SnapshotBufferComponent& buffer, double renderTick,
                                                        double tickRate, double maxExtrapolationTicks) {
    const auto& samples = buffer.samples;
    if (samples.empty()) return std::nullopt;
    if (renderTick <= static_cast<double>(samples.front().tick)) return samples.front().position;

    const auto after = std::upper_bound(samples.begin(), samples.end(), renderTick,
                                        [](double tick, const SnapshotSample& stored) {
                                          return tick < static_cast<double>(stored.tick);
                                        });
    if (after == samples.end()) {
      const SnapshotSample& last = samples.back();
      const double ahead = std::min(renderTick - static_cast<double>(last.tick), std::max(maxExtrapolationTicks, 0.0));
      const double seconds = tickRate > 0.0 ? ahead / tickRate : 0.0;
      return Vector2f{last.position.x + static_cast<float>(static_cast<double>(last.velocity.x) * seconds),
                      last.position.y + static_cast<float>(static_cast<double>(last.velocity.y) * seconds)};
    }

    const SnapshotSample& to = *after;
    const SnapshotSample& from = *(after - 1);
    const double span = static_cast<double>(to.tick - from.tick);
    const float alpha = static_cast<float>((renderTick - static_cast<double>(from.tick)) / span);
    return Vector2f{from.position.x + (to.position.x - from.position.x) * alpha,
                    from.position.y + (to.position.y - from.position.y) * alpha};
  }
}

#endif // !RTYPE_ENGINE_SNAPSHOTBUFFER_HPP
