#ifndef RTYPE_BACKEND_SFMLSPRITE_HPP
#define RTYPE_BACKEND_SFMLSPRITE_HPP

#include "engine/gfx/ISprite.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <memory>
#include <optional>

namespace rtype::backend {
  class SFMLSprite : public engine::ISprite {
  public:
    explicit SFMLSprite(std::shared_ptr<engine::ITexture> texture);
    ~SFMLSprite() override = default;

    void setTexture(std::shared_ptr<engine::ITexture> texture) override;
    void setPosition(const engine::Vector2f& position) override;
    engine::Vector2f getPosition() const override;

    void setRotation(float angle) override;
    float getRotation() const override;

    void setScale(const engine::Vector2f& scale) override;
    engine::Vector2f getScale() const override;

    void setTextureRect(const engine::IntRect& rect) override;
    engine::IntRect getTextureRect() const override;

    void setColor(const engine::Color& color) override;

    const sf::Sprite* getNativeSprite() const { return _sprite ? &_sprite.value() : nullptr; }

  private:
    std::optional<sf::Sprite> _sprite;
    std::shared_ptr<engine::ITexture> _textureOwner;
  };
}

#endif // !RTYPE_BACKEND_SFMLSPRITE_HPP
