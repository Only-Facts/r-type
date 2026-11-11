#ifndef RTYPE_NETWORK_SNAPSHOTDELTA_HPP
#define RTYPE_NETWORK_SNAPSHOTDELTA_HPP

#include "exception.hpp"
#include "network/PacketStream.hpp"
#include "network/Protocol.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rtype::network {
  inline constexpr std::uint8_t DELTA_POS_X = 0x01;
  inline constexpr std::uint8_t DELTA_POS_Y = 0x02;
  inline constexpr std::uint8_t DELTA_VEL_X = 0x04;
  inline constexpr std::uint8_t DELTA_VEL_Y = 0x08;
  inline constexpr std::uint8_t DELTA_ALL_FIELDS = DELTA_POS_X | DELTA_POS_Y | DELTA_VEL_X | DELTA_VEL_Y;

  using SnapshotState = std::unordered_map<NetworkId, PackedEntityStateData>;

  struct SnapshotDeltaRecord {
    std::uint8_t mask{0};
    PackedEntityStateData state{};
  };

  struct SnapshotDelta {
    std::uint32_t tick{0};
    std::uint32_t baseTick{0};
    std::vector<SnapshotDeltaRecord> changed;
    std::vector<NetworkId> removed;
  };

  inline std::uint8_t snapshotDeltaMask(const PackedEntityStateData& base, const PackedEntityStateData& current) {
    std::uint8_t mask = 0;
    if (base.posX != current.posX) mask |= DELTA_POS_X;
    if (base.posY != current.posY) mask |= DELTA_POS_Y;
    if (base.velX != current.velX) mask |= DELTA_VEL_X;
    if (base.velY != current.velY) mask |= DELTA_VEL_Y;
    return mask;
  }

  inline SnapshotDelta makeSnapshotDelta(const SnapshotState& base, std::uint32_t baseTick,
                                         const SnapshotState& current, std::uint32_t tick) {
    SnapshotDelta delta;
    delta.tick = tick;
    delta.baseTick = baseTick;
    for (const auto& [id, state] : current) {
      const auto previous = base.find(id);
      const std::uint8_t mask = previous == base.end() ? DELTA_ALL_FIELDS : snapshotDeltaMask(previous->second, state);
      if (mask != 0) delta.changed.push_back({mask, state});
    }
    for (const auto& [id, state] : base)
      if (current.find(id) == current.end()) delta.removed.push_back(id);
    return delta;
  }

  inline void writeSnapshotDelta(PacketStreamWriter& writer, const SnapshotDelta& delta) {
    writer << delta.tick << delta.baseTick << static_cast<std::uint16_t>(delta.changed.size());
    for (const auto& record : delta.changed) {
      writer << record.state.netId << record.mask;
      if (record.mask & DELTA_POS_X) writer << record.state.posX;
      if (record.mask & DELTA_POS_Y) writer << record.state.posY;
      if (record.mask & DELTA_VEL_X) writer << record.state.velX;
      if (record.mask & DELTA_VEL_Y) writer << record.state.velY;
    }
    writer << static_cast<std::uint16_t>(delta.removed.size());
    for (const NetworkId id : delta.removed) writer << id;
  }

  inline SnapshotDelta readSnapshotDelta(PacketStreamReader& reader, std::size_t maxRecords) {
    SnapshotDelta delta;
    std::uint16_t changedCount{0};
    reader >> delta.tick >> delta.baseTick >> changedCount;
    if (changedCount > maxRecords) throw RtypeError("snapshot delta: too many changed entities");
    delta.changed.reserve(changedCount);
    for (std::uint16_t i = 0; i < changedCount; ++i) {
      SnapshotDeltaRecord record;
      reader >> record.state.netId >> record.mask;
      if (record.mask == 0 || (record.mask & ~DELTA_ALL_FIELDS) != 0)
        throw RtypeError("snapshot delta: invalid field mask");
      if (record.mask & DELTA_POS_X) reader >> record.state.posX;
      if (record.mask & DELTA_POS_Y) reader >> record.state.posY;
      if (record.mask & DELTA_VEL_X) reader >> record.state.velX;
      if (record.mask & DELTA_VEL_Y) reader >> record.state.velY;
      delta.changed.push_back(record);
    }
    std::uint16_t removedCount{0};
    reader >> removedCount;
    if (removedCount > maxRecords) throw RtypeError("snapshot delta: too many removed entities");
    delta.removed.resize(removedCount);
    for (auto& id : delta.removed) reader >> id;
    if (reader.hasMoreData()) throw RtypeError("snapshot delta: trailing bytes");
    return delta;
  }

  inline SnapshotState applySnapshotDelta(const SnapshotState& base, const SnapshotDelta& delta) {
    SnapshotState result = base;
    for (const NetworkId id : delta.removed) result.erase(id);
    for (const auto& record : delta.changed) {
      auto [it, inserted] = result.try_emplace(record.state.netId, record.state);
      if (inserted && record.mask != DELTA_ALL_FIELDS)
        throw RtypeError("snapshot delta: partial record for an unknown entity");
      PackedEntityStateData& state = it->second;
      if (record.mask & DELTA_POS_X) state.posX = record.state.posX;
      if (record.mask & DELTA_POS_Y) state.posY = record.state.posY;
      if (record.mask & DELTA_VEL_X) state.velX = record.state.velX;
      if (record.mask & DELTA_VEL_Y) state.velY = record.state.velY;
    }
    return result;
  }
}

#endif // !RTYPE_NETWORK_SNAPSHOTDELTA_HPP
