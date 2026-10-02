#include "gfx/SFMLSprite.hpp"
#include "gfx/SFMLTexture.hpp"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>

namespace rtype::engine {
  SFMLSprite::SFMLSprite(std::shared_ptr<ITexture> texture) {
    setTexture(texture);
  }

  void SFMLSprite::setTexture(std::shared_ptr<ITexture> texture) {
    _textureOwner = texture;
    auto sfmlTex = std::dynamic_pointer_cast<SFMLTexture>(texture);
    if (sfmlTex) {
      _sprite.emplace(sfmlTex->getNativeTexture());
    }
  }

  void SFMLSprite::setPosition(const Vector2f& position) {
    if (_sprite) _sprite->setPosition({position.x, position.y});
  }

  Vector2f SFMLSprite::getPosition() const {
    if (!_sprite) return Vector2f{0.0f, 0.0f};
    sf::Vector2f pos = _sprite->getPosition();
    return Vector2f{pos.x, pos.y};
  }

  void SFMLSprite::setRotation(float angle) {
    if (_sprite) _sprite->setRotation(sf::degrees(angle));
  }

  float SFMLSprite::getRotation() const {
    return _sprite ? _sprite->getRotation().asDegrees() : 0.0f;
  }

  void SFMLSprite::setScale(const Vector2f& scale) {
    if (_sprite) _sprite->setScale(sf::Vector2f(scale.x, scale.y));
  }

  Vector2f SFMLSprite::getScale() const {
    if (!_sprite) return Vector2f{1.0f, 1.0f};
    sf::Vector2f scale = _sprite->getScale();
    return Vector2f{scale.x, scale.y};
  }

  void SFMLSprite::setTextureRect(const IntRect& rect) {
    if (_sprite)
      _sprite->setTextureRect(sf::Rect<int>({rect.left, rect.top}, {rect.width, rect.height}));
  }

  IntRect SFMLSprite::getTextureRect() const {
    if (!_sprite) return IntRect{0, 0, 0, 0};
    sf::Rect<int> rect = _sprite->getTextureRect();
    return IntRect{rect.position.x, rect.position.y, rect.size.x, rect.size.y};
  }

  void SFMLSprite::setColor(const Color& color) {
    if (_sprite) _sprite->setColor(sf::Color(color.r, color.g, color.b, color.a));
  }
}
