#ifndef SFMLENEMYSPRITEFACTORY_HPP
#define SFMLENEMYSPRITEFACTORY_HPP

#include "IEnemySpriteFactory.hpp"
#include "gfx/ITextureManager.hpp"

namespace rtype::client {
  class SFMLEnemySpriteFactory : public IEnemySpriteFactory {
  public:
    explicit SFMLEnemySpriteFactory(engine::ITextureManager& textureManager);
    ~SFMLEnemySpriteFactory() override = default;

    std::shared_ptr<engine::ISprite> createSprite(game::EnemyType type) override;
    engine::AnimationComponent createAnimation(game::EnemyType type) const override;
    engine::Vector2f getSpriteScale() const override;

  private:
    engine::ITextureManager& _textureManager;
  };
}

#endif // !SFMLENEMYSPRITEFACTORY_HPP
