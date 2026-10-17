#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>

namespace rtype::network {
  constexpr std::uint16_t PROTOCOL_MAGIC = 0x5254;

  enum class PacketType : std::uint8_t {
    ConnectReq    = 0x01,
    ConnectAck    = 0x02,
    SpawnPlayer   = 0x10,
    DestroyEntity = 0x11,
    SpawnMissile  = 0x12,
    PlayerInput   = 0x20,
    GameStateSync = 0x30,
    Ping          = 0xE0,
    Pong          = 0xE1
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

  struct MissileSpawnData {
    NetworkId missileId;
    NetworkId ownerId;
    float posX;
    float posY;
    float velX;
    float velY;
  };

#pragma pack(push, 1)
  struct EnemyStateData {
    NetworkId enemyId;
    std::uint8_t enemyType;
    float posX;
    float posY;
  };
#pragma pack(pop)
}

#endif // !PROTOCOL_HPP
