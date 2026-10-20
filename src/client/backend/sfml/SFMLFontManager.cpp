#include "backend/sfml/SFMLFontManager.hpp"
#include "backend/sfml/SFMLText.hpp"
#include "exception.hpp"

namespace rtype::backend {
  void SFMLFontManager::loadFont(const std::string& filepath) {
    getFont(filepath);
  }

  std::shared_ptr<engine::IText> SFMLFontManager::createText(const std::string& filepath) {
    return std::make_shared<SFMLText>(getFont(filepath));
  }

  void SFMLFontManager::clear() {
    _cache.clear();
  }

  std::shared_ptr<const sf::Font> SFMLFontManager::getFont(const std::string& filepath) {
    auto it = _cache.find(filepath);
    if (it != _cache.end()) return it->second;

    auto font = std::make_shared<sf::Font>();
    if (!font->openFromFile(filepath)) throw RtypeError("Failed to load font from file: " + filepath);

    _cache[filepath] = font;
    return font;
  }
}
