#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rtype::network {
  constexpr std::uint16_t PROTOCOL_MAGIC = 0x5254;

  enum class PacketType : std::uint8_t {
    ConnectReq = 0x01,
    ConnectAck = 0x02,
    SpawnPlayer = 0x10,
    DestroyEntity = 0x11,
    PlayerInput = 0x20,
    GameStateSync = 0x30,
    Ping = 0xE0,
    Disconnect = 0x03,
    ConnectReject = 0x04,
    Ack = 0x05,
    Pong = 0xE1,
    RoomListReq = 0x40,
    RoomListAck = 0x41,
    RoomCreateReq = 0x42,
    RoomCreateAck = 0x43,
    RoomJoinReq = 0x44,
    RoomJoinAck = 0x45,
    RoomLeaveReq = 0x46,
    RoomLeaveAck = 0x47,
    SpawnEntity = 0x12,
    HealthUpdate = 0x13,
    AugmentUpdate = 0x14,
    ScoreUpdate = 0x15,
    GameStateSyncPacked = 0x31,
    GameStateSyncDelta = 0x32,
    InputAck = 0x33,
    SnapshotRequest = 0x34
  };

  #pragma pack(push, 1)
  struct PacketHeader {
    std::uint16_t magic{PROTOCOL_MAGIC};
    PacketType type;
    std::uint32_t sequence{0};
    std::uint16_t payloadSize{0};
  };
  #pragma pack(pop)

  using NetworkId = std::uint32_t;

  struct EntityStateData {
    NetworkId netId;
    float posX;
    float posY;
    float velX;
    float velY;
  };

  struct PackedEntityStateData {
    NetworkId netId;
    std::int16_t posX;
    std::int16_t posY;
    std::int16_t velX;
    std::int16_t velY;

    friend bool operator==(const PackedEntityStateData&,
                           const PackedEntityStateData&) = default;
  };

  static_assert(sizeof(PacketHeader) == 9);
  static_assert(sizeof(EntityStateData) == 20);
  static_assert(sizeof(PackedEntityStateData) == 12);

  inline std::int16_t quantizeSnapshotValue(float value, float scale) {
    if (!std::isfinite(value)) return 0;
    const double scaled = std::round(static_cast<double>(value) * scale);
    const double bounded = std::clamp(
      scaled,
      static_cast<double>(std::numeric_limits<std::int16_t>::min()),
      static_cast<double>(std::numeric_limits<std::int16_t>::max()));
    return static_cast<std::int16_t>(bounded);
  }

  inline PackedEntityStateData packEntityState(const EntityStateData& state) {
    return PackedEntityStateData{
      state.netId,
      quantizeSnapshotValue(state.posX, 8.0f),
      quantizeSnapshotValue(state.posY, 8.0f),
      quantizeSnapshotValue(state.velX, 4.0f),
      quantizeSnapshotValue(state.velY, 4.0f)
    };
  }

  inline EntityStateData unpackEntityState(
      const PackedEntityStateData& state) {
    return EntityStateData{
      state.netId,
      static_cast<float>(state.posX) / 8.0f,
      static_cast<float>(state.posY) / 8.0f,
      static_cast<float>(state.velX) / 4.0f,
      static_cast<float>(state.velY) / 4.0f
    };
  }
}

#endif // !PROTOCOL_HPP
