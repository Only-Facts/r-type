#include "backend/sfml/SFMLTextureManager.hpp"
#include "backend/sfml/SFMLSprite.hpp"
#include "backend/sfml/SFMLTexture.hpp"
#include "exception.hpp"

namespace rtype::backend {
  std::shared_ptr<engine::ITexture> SFMLTextureManager::loadTexture(const std::string& filepath) {
    auto it = _cache.find(filepath);
    if (it != _cache.end()) return it->second;

    auto texture = std::make_shared<SFMLTexture>();
    if (!texture->loadFromFile(filepath)) throw RtypeError("Failed to load texture from file: " + filepath);

    _cache[filepath] = texture;
    return texture;
  }

  std::shared_ptr<engine::ISprite> SFMLTextureManager::createSprite(const std::string& filepath) {
    return std::make_shared<SFMLSprite>(loadTexture(filepath));
  }

  void SFMLTextureManager::clear() {
    _cache.clear();
  }
}
