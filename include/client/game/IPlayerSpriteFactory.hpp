#ifndef IPLAYERSPRITEFACTORY_HPP
#define IPLAYERSPRITEFACTORY_HPP

#include "ecs/Components.hpp"
#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include "network/Protocol.hpp"
#include <memory>

namespace rtype::client {
  class IPlayerSpriteFactory {
  public:
    virtual ~IPlayerSpriteFactory() = default;

    virtual std::shared_ptr<engine::ISprite> createSprite(network::NetworkId playerId) = 0;
    virtual engine::ShipTiltComponent createTilt(network::NetworkId playerId) const = 0;
    virtual engine::Vector2f getSpriteScale() const = 0;
    virtual engine::Color getPlayerColor(network::NetworkId playerId) const = 0;
  };
}

#endif // !IPLAYERSPRITEFACTORY_HPP
