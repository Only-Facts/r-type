#include "game/SFMLEnemySpriteFactory.hpp"
#include "gfx/SFMLSprite.hpp"
#include <array>

namespace rtype::client {
  namespace {
    struct EnemyVisual {
      const char* texturePath;
      int frameWidth;
      int frameHeight;
      int frameCount;
      float frameDuration;
      bool pingPong;
    };

    constexpr std::array<EnemyVisual, static_cast<std::size_t>(game::EnemyType::Count)> ENEMY_VISUALS{{
      {"assets/sprites/enemy_spinner.png", 16, 15, 6, 0.08f, false},
    }};

    constexpr float SPRITE_SCALE = 3.0f;

    const EnemyVisual& getVisual(game::EnemyType type) {
      return ENEMY_VISUALS[static_cast<std::size_t>(type)];
    }
  }

  SFMLEnemySpriteFactory::SFMLEnemySpriteFactory(engine::ITextureManager& textureManager)
    : _textureManager(textureManager) {}

  std::shared_ptr<engine::ISprite> SFMLEnemySpriteFactory::createSprite(game::EnemyType type) {
    const EnemyVisual& visual = getVisual(type);
    auto texture = _textureManager.loadTexture(visual.texturePath);
    auto sprite = std::make_shared<engine::SFMLSprite>(texture);

    sprite->setTextureRect(engine::IntRect{0, 0, visual.frameWidth, visual.frameHeight});
    return sprite;
  }

  engine::AnimationComponent SFMLEnemySpriteFactory::createAnimation(game::EnemyType type) const {
    const EnemyVisual& visual = getVisual(type);

    engine::AnimationComponent animation{};
    animation.frameWidth = visual.frameWidth;
    animation.frameHeight = visual.frameHeight;
    animation.frameCount = visual.frameCount;
    animation.frameDuration = visual.frameDuration;
    animation.pingPong = visual.pingPong;
    return animation;
  }

  engine::Vector2f SFMLEnemySpriteFactory::getSpriteScale() const {
    return engine::Vector2f{SPRITE_SCALE, SPRITE_SCALE};
  }
}
