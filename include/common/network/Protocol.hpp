#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>

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
    ScoreUpdate = 0x15
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
}

#endif // !PROTOCOL_HPP
