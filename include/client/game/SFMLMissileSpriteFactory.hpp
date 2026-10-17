#ifndef SFMLMISSILESPRITEFACTORY_HPP
#define SFMLMISSILESPRITEFACTORY_HPP

#include "IMissileSpriteFactory.hpp"
#include "gfx/ITextureManager.hpp"
#include "gfx/SpriteSheet.hpp"

namespace rtype::client {
  class SFMLMissileSpriteFactory : public IMissileSpriteFactory {
  public:
    explicit SFMLMissileSpriteFactory(engine::ITextureManager& textureManager);
    ~SFMLMissileSpriteFactory() override = default;

    std::shared_ptr<engine::ISprite> createSprite() override;
    engine::Vector2f getSpriteScale() const override;

  private:
    engine::ITextureManager& _textureManager;
    engine::SpriteSheet _sheet;
  };
}

#endif // !SFMLMISSILESPRITEFACTORY_HPP
