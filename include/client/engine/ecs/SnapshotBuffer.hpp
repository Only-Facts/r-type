#ifndef RTYPE_ENGINE_SNAPSHOTBUFFER_HPP
#define RTYPE_ENGINE_SNAPSHOTBUFFER_HPP

#include "engine/ecs/Components.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>

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
}

#endif // !RTYPE_ENGINE_SNAPSHOTBUFFER_HPP
