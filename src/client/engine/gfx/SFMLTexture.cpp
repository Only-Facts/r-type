#include "gfx/SFMLTexture.hpp"

namespace rtype::engine {
  bool SFMLTexture::loadFromFile(const std::string& filepath) {
      return _texture.loadFromFile(filepath);
  }

  Vector2u SFMLTexture::getSize() const {
      sf::Vector2u size = _texture.getSize();
      return Vector2u{size.x, size.y};
  }
}
