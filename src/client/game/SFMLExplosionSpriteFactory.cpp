#include "game/SFMLExplosionSpriteFactory.hpp"
#include "gfx/SFMLSprite.hpp"

namespace rtype::client {
  namespace {
    constexpr const char* TEXTURE_PATH = "assets/sprites/explosion.png";
    constexpr int FRAME_WIDTH = 16;
    constexpr int FRAME_HEIGHT = 14;
    constexpr int FRAME_COUNT = 7;
    constexpr float FRAME_DURATION = 0.07f;
  }

  SFMLExplosionSpriteFactory::SFMLExplosionSpriteFactory(engine::ITextureManager& textureManager)
    : _textureManager(textureManager) {}

  std::shared_ptr<engine::ISprite> SFMLExplosionSpriteFactory::createSprite() {
    auto texture = _textureManager.loadTexture(TEXTURE_PATH);
    auto sprite = std::make_shared<engine::SFMLSprite>(texture);

    sprite->setTextureRect(engine::IntRect{0, 0, FRAME_WIDTH, FRAME_HEIGHT});
    return sprite;
  }

  engine::AnimationComponent SFMLExplosionSpriteFactory::createAnimation() const {
    engine::AnimationComponent animation{};
    animation.frameWidth = FRAME_WIDTH;
    animation.frameHeight = FRAME_HEIGHT;
    animation.frameCount = FRAME_COUNT;
    animation.frameDuration = FRAME_DURATION;
    animation.loop = false;
    return animation;
  }

  engine::Vector2f SFMLExplosionSpriteFactory::getFrameSize() const {
    return engine::Vector2f{static_cast<float>(FRAME_WIDTH), static_cast<float>(FRAME_HEIGHT)};
  }

  float SFMLExplosionSpriteFactory::getDuration() const {
    return FRAME_COUNT * FRAME_DURATION;
  }
}
