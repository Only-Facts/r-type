#ifndef SFMLEXPLOSIONSPRITEFACTORY_HPP
#define SFMLEXPLOSIONSPRITEFACTORY_HPP

#include "IExplosionSpriteFactory.hpp"
#include "gfx/ITextureManager.hpp"

namespace rtype::client {
  class SFMLExplosionSpriteFactory : public IExplosionSpriteFactory {
  public:
    explicit SFMLExplosionSpriteFactory(engine::ITextureManager& textureManager);
    ~SFMLExplosionSpriteFactory() override = default;

    std::shared_ptr<engine::ISprite> createSprite() override;
    engine::AnimationComponent createAnimation() const override;
    engine::Vector2f getFrameSize() const override;
    float getDuration() const override;

  private:
    engine::ITextureManager& _textureManager;
  };
}

#endif // !SFMLEXPLOSIONSPRITEFACTORY_HPP
