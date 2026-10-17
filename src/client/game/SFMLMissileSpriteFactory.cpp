#include "game/SFMLMissileSpriteFactory.hpp"
#include "gfx/SFMLSprite.hpp"

namespace rtype::client {
  namespace {
    constexpr const char* TEXTURE_PATH = "assets/sprites/player_missiles.png";
    constexpr int FRAME_WIDTH = 16;
    constexpr int FRAME_HEIGHT = 12;
    constexpr int COLUMN_COUNT = 4;
    constexpr int ROW_COUNT = 1;
    constexpr int MISSILE_COLUMN = 1;
    constexpr float SPRITE_SCALE = 3.0f;
  }

  SFMLMissileSpriteFactory::SFMLMissileSpriteFactory(engine::ITextureManager& textureManager)
    : _textureManager(textureManager),
      _sheet(FRAME_WIDTH, FRAME_HEIGHT, COLUMN_COUNT, ROW_COUNT) {}

  std::shared_ptr<engine::ISprite> SFMLMissileSpriteFactory::createSprite() {
    auto texture = _textureManager.loadTexture(TEXTURE_PATH);
    auto sprite = std::make_shared<engine::SFMLSprite>(texture);

    sprite->setTextureRect(_sheet.getFrameRect(MISSILE_COLUMN, 0));
    return sprite;
  }

  engine::Vector2f SFMLMissileSpriteFactory::getSpriteScale() const {
    return engine::Vector2f{SPRITE_SCALE, SPRITE_SCALE};
  }
}
