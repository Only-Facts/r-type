#include "backend/sfml/SFMLSprite.hpp"
#include "backend/sfml/SFMLTexture.hpp"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>

namespace rtype::backend {
  SFMLSprite::SFMLSprite(std::shared_ptr<engine::ITexture> texture) {
    setTexture(std::move(texture));
  }

  void SFMLSprite::setTexture(std::shared_ptr<engine::ITexture> texture) {
    auto sfmlTexture = std::dynamic_pointer_cast<SFMLTexture>(texture);
    if (!sfmlTexture) return;
    _textureOwner = std::move(texture);
    _sprite.emplace(sfmlTexture->getNativeTexture());
  }

  void SFMLSprite::setPosition(const engine::Vector2f& position) {
    if (_sprite) _sprite->setPosition({position.x, position.y});
  }

  engine::Vector2f SFMLSprite::getPosition() const {
    if (!_sprite) return engine::Vector2f{0.0f, 0.0f};
    const sf::Vector2f pos = _sprite->getPosition();
    return engine::Vector2f{pos.x, pos.y};
  }

  void SFMLSprite::setRotation(float angle) {
    if (_sprite) _sprite->setRotation(sf::degrees(angle));
  }

  float SFMLSprite::getRotation() const {
    return _sprite ? _sprite->getRotation().asDegrees() : 0.0f;
  }

  void SFMLSprite::setScale(const engine::Vector2f& scale) {
    if (_sprite) _sprite->setScale({scale.x, scale.y});
  }

  engine::Vector2f SFMLSprite::getScale() const {
    if (!_sprite) return engine::Vector2f{1.0f, 1.0f};
    const sf::Vector2f scale = _sprite->getScale();
    return engine::Vector2f{scale.x, scale.y};
  }

  void SFMLSprite::setTextureRect(const engine::IntRect& rect) {
    if (_sprite) _sprite->setTextureRect(sf::IntRect({rect.left, rect.top}, {rect.width, rect.height}));
  }

  engine::IntRect SFMLSprite::getTextureRect() const {
    if (!_sprite) return engine::IntRect{0, 0, 0, 0};
    const sf::IntRect rect = _sprite->getTextureRect();
    return engine::IntRect{rect.position.x, rect.position.y, rect.size.x, rect.size.y};
  }

  void SFMLSprite::setColor(const engine::Color& color) {
    if (_sprite) _sprite->setColor(sf::Color(color.r, color.g, color.b, color.a));
  }
}
