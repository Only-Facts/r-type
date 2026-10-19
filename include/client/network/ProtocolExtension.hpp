#ifndef RTYPE_CLIENT_PROTOCOLEXTENSION_HPP
#define RTYPE_CLIENT_PROTOCOLEXTENSION_HPP

#include "network/Protocol.hpp"
#include <cstdint>



namespace rtype::network::ext {
  constexpr PacketType SPAWN_ENTITY = static_cast<PacketType>(0x12);


  constexpr PacketType HEALTH_UPDATE = static_cast<PacketType>(0x13);
  constexpr PacketType AUGMENT_UPDATE = static_cast<PacketType>(0x14);

  using AugmentId = std::uint8_t;
  using EntityType = std::uint8_t;
  constexpr EntityType ENTITY_PLAYER = 0;
}

#endif // !RTYPE_CLIENT_PROTOCOLEXTENSION_HPP
