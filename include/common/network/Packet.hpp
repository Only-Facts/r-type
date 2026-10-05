#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <vector>

namespace rtype::network {
  enum class Command : std::uint8_t {
    Connect = 0x01,
    Disconnect = 0x02,
    PlayerInput = 0x10,
    GameState = 0x20
  };

  #pragma pack(push, 1)
  struct PacketHeader {
    Command command;
    std::uint16_t payloadSize;
    std::uint8_t reserved;
  };
  #pragma pack(pop)

  using PacketData = std::vector<std::uint8_t>;
}

#endif // !PACKET_HPP
