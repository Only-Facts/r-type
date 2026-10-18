#ifndef SFMLPLAYERSPRITEFACTORY_HPP
#define SFMLPLAYERSPRITEFACTORY_HPP

#include "IPlayerSpriteFactory.hpp"
#include "gfx/ITextureManager.hpp"
#include "gfx/SpriteSheet.hpp"

namespace rtype::client {
  class SFMLPlayerSpriteFactory : public IPlayerSpriteFactory {
  public:
    explicit SFMLPlayerSpriteFactory(engine::ITextureManager& textureManager);
    ~SFMLPlayerSpriteFactory() override = default;

    std::shared_ptr<engine::ISprite> createSprite(network::NetworkId playerId) override;
    engine::ShipTiltComponent createTilt(network::NetworkId playerId) const override;
    engine::Vector2f getSpriteScale() const override;
    engine::Color getPlayerColor(network::NetworkId playerId) const override;

  private:
    int getSheetRowForPlayer(network::NetworkId playerId) const;

    engine::ITextureManager& _textureManager;
    engine::SpriteSheet _sheet;
  };
}

#endif // !SFMLPLAYERSPRITEFACTORY_HPP
