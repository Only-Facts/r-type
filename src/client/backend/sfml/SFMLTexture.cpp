#include "backend/sfml/SFMLTexture.hpp"

namespace rtype::backend {
  bool SFMLTexture::loadFromFile(const std::string& filepath) {
    return _texture.loadFromFile(filepath);
  }

  engine::Vector2u SFMLTexture::getSize() const {
    const sf::Vector2u size = _texture.getSize();
    return engine::Vector2u{size.x, size.y};
  }
}
