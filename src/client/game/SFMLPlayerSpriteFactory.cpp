#include "game/SFMLPlayerSpriteFactory.hpp"
#include "gfx/SFMLSprite.hpp"

namespace rtype::client {
  namespace {
    constexpr const char* TEXTURE_PATH = "assets/sprites/player_ships.png";
    constexpr int FRAME_WIDTH = 33;
    constexpr int FRAME_HEIGHT = 17;
    constexpr int COLUMN_COUNT = 5;
    constexpr int ROW_COUNT = 5;
    constexpr int IDLE_COLUMN = 2;
    constexpr float SPRITE_SCALE = 3.0f;
  }

  SFMLPlayerSpriteFactory::SFMLPlayerSpriteFactory(engine::ITextureManager& textureManager)
    : _textureManager(textureManager),
      _sheet(FRAME_WIDTH, FRAME_HEIGHT, COLUMN_COUNT, ROW_COUNT) {}

  std::shared_ptr<engine::ISprite> SFMLPlayerSpriteFactory::createSprite(network::NetworkId playerId) {
    auto texture = _textureManager.loadTexture(TEXTURE_PATH);
    auto sprite = std::make_shared<engine::SFMLSprite>(texture);

    sprite->setTextureRect(_sheet.getFrameRect(IDLE_COLUMN, getSheetRowForPlayer(playerId)));
    return sprite;
  }

  engine::Vector2f SFMLPlayerSpriteFactory::getSpriteScale() const {
    return engine::Vector2f{SPRITE_SCALE, SPRITE_SCALE};
  }

  // Player 1 -> row 0, player 2 -> row 1, ... then it loops.
  int SFMLPlayerSpriteFactory::getSheetRowForPlayer(network::NetworkId playerId) const {
    if (playerId == 0) return 0;
    return static_cast<int>((playerId - 1) % static_cast<network::NetworkId>(_sheet.getRowCount()));
  }
}
