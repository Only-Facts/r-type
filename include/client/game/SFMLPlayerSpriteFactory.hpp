#ifndef SFMLPLAYERSPRITEFACTORY_HPP
#define SFMLPLAYERSPRITEFACTORY_HPP

#include "IPlayerSpriteFactory.hpp"
#include "gfx/ITextureManager.hpp"
#include "gfx/SpriteSheet.hpp"

namespace rtype::client {
  // Draws each player with its own color, taken from assets/sprites/player_ships.png
  // (5 columns of banking frames x 5 rows, one row per player).
  class SFMLPlayerSpriteFactory : public IPlayerSpriteFactory {
  public:
    explicit SFMLPlayerSpriteFactory(engine::ITextureManager& textureManager);
    ~SFMLPlayerSpriteFactory() override = default;

    std::shared_ptr<engine::ISprite> createSprite(network::NetworkId playerId) override;
    engine::Vector2f getSpriteScale() const override;

  private:
    int getSheetRowForPlayer(network::NetworkId playerId) const;

    engine::ITextureManager& _textureManager;
    engine::SpriteSheet _sheet;
  };
}

#endif // !SFMLPLAYERSPRITEFACTORY_HPP
