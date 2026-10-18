#ifndef RTYPE_CLIENT_PROTOCOLEXTENSION_HPP
#define RTYPE_CLIENT_PROTOCOLEXTENSION_HPP

#include "network/Protocol.hpp"
#include <cstdint>

// Client side of the protocol extension proposed in docs/protocol-extension.md.
// Kept in the client until the team merges it into common/network/Protocol.hpp;
// then this file only has to include Protocol.hpp.
//
// Backward compatible: a server that does not know it never sends SpawnEntity, and
// it ignores the extra "shoot" byte at the end of PlayerInput.

namespace rtype::network::ext {
  /// Server -> client. Payload: NetworkId netId, EntityType type, float posX, float posY.
  constexpr PacketType SPAWN_ENTITY = static_cast<PacketType>(0x12);

  /// Gameplay type of a networked entity. The client maps each id to a look and
  /// sounds in the [entity_types] config section, so new enemies need no client code.
  using EntityType = std::uint8_t;
  constexpr EntityType ENTITY_PLAYER = 0;
}

#endif // !RTYPE_CLIENT_PROTOCOLEXTENSION_HPP
