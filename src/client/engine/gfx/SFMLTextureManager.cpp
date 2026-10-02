#include "gfx/SFMLTextureManager.hpp"
#include "gfx/SFMLTexture.hpp"
#include "exception.hpp"

namespace rtype::engine {
  std::shared_ptr<ITexture> SFMLTextureManager::loadTexture(const std::string& filepath) {
    auto it = _cache.find(filepath);
    if (it != _cache.end()) return it->second;

    auto texture = std::make_shared<SFMLTexture>();
    if (!texture->loadFromFile(filepath)) throw RtypeError("Failed to load texture from file: " + filepath);

    _cache[filepath] = texture;
    return texture;
  }

  void SFMLTextureManager::clear() {
    _cache.clear();
  }
}
