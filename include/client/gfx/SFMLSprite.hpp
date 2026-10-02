#ifndef SFMLSPRITE_HPP
#define SFMLSPRITE_HPP

#include "ISprite.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <optional>

namespace rtype::engine {
  class SFMLSprite : public ISprite {
  public:
    explicit SFMLSprite(std::shared_ptr<ITexture> texture);
    ~SFMLSprite() override = default;

    void setTexture(std::shared_ptr<ITexture> texture) override;
    void setPosition(const Vector2f& position) override;
    Vector2f getPosition() const override;

    void setRotation(float angle) override;
    float getRotation() const override;

    void setScale(const Vector2f& scale) override;
    Vector2f getScale() const override;

    void setTextureRect(const IntRect& rect) override;
    IntRect getTextureRect() const override;

    void setColor(const Color& color) override;

    const sf::Sprite* getNativeSprite() const { return _sprite ? &_sprite.value() : nullptr; }

  private:
    std::optional<sf::Sprite> _sprite;
    std::shared_ptr<ITexture> _textureOwner;
  };
}

#endif // !SFMLSPRITE_HPP
