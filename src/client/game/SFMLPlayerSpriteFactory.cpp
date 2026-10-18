#include "game/SFMLPlayerSpriteFactory.hpp"
#include "gfx/SFMLSprite.hpp"
#include <array>

namespace rtype::client {
  namespace {
    constexpr const char* TEXTURE_PATH = "assets/sprites/player_ships.png";
    constexpr int FRAME_WIDTH = 33;
    constexpr int FRAME_HEIGHT = 17;
    constexpr int COLUMN_COUNT = 5;
    constexpr int ROW_COUNT = 5;
    constexpr int IDLE_COLUMN = 2;
    constexpr int MAX_TILT = 2;
    constexpr float SPRITE_SCALE = 3.0f;

    constexpr std::array<engine::Color, ROW_COUNT> ROW_COLORS{{
      {80, 230, 255, 255},
      {255, 130, 255, 255},
      {110, 255, 110, 255},
      {255, 110, 100, 255},
      {110, 150, 255, 255},
    }};
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

  engine::ShipTiltComponent SFMLPlayerSpriteFactory::createTilt(network::NetworkId playerId) const {
    engine::ShipTiltComponent tilt{};
    tilt.frameWidth = FRAME_WIDTH;
    tilt.frameHeight = FRAME_HEIGHT;
    tilt.row = getSheetRowForPlayer(playerId);
    tilt.neutralColumn = IDLE_COLUMN;
    tilt.maxTilt = MAX_TILT;
    return tilt;
  }

  engine::Vector2f SFMLPlayerSpriteFactory::getSpriteScale() const {
    return engine::Vector2f{SPRITE_SCALE, SPRITE_SCALE};
  }

  engine::Color SFMLPlayerSpriteFactory::getPlayerColor(network::NetworkId playerId) const {
    return ROW_COLORS[static_cast<std::size_t>(getSheetRowForPlayer(playerId))];
  }

  int SFMLPlayerSpriteFactory::getSheetRowForPlayer(network::NetworkId playerId) const {
    if (playerId == 0) return 0;
    return static_cast<int>((playerId - 1) % static_cast<network::NetworkId>(_sheet.getRowCount()));
  }
}
