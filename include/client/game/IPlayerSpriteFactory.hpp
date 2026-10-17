#ifndef IPLAYERSPRITEFACTORY_HPP
#define IPLAYERSPRITEFACTORY_HPP

#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include "network/Protocol.hpp"
#include <memory>

namespace rtype::client {
  // Creates the sprite used to draw a player's ship.
  // Lets the network layer spawn players without knowing about SFML or sprite sheets.
  class IPlayerSpriteFactory {
  public:
    virtual ~IPlayerSpriteFactory() = default;

    virtual std::shared_ptr<engine::ISprite> createSprite(network::NetworkId playerId) = 0;
    virtual engine::Vector2f getSpriteScale() const = 0;
  };
}

#endif // !IPLAYERSPRITEFACTORY_HPP
