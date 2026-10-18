#include "gfx/SFMLTextFactory.hpp"
#include "gfx/SFMLText.hpp"
#include "exception.hpp"

namespace rtype::engine {
  SFMLTextFactory::SFMLTextFactory(const std::string& fontPath) : _font(std::make_shared<sf::Font>()) {
    if (!_font->openFromFile(fontPath)) throw RtypeError("Failed to load font from file: " + fontPath);
  }

  std::shared_ptr<IText> SFMLTextFactory::createText(unsigned int characterSize) {
    return std::make_shared<SFMLText>(_font, characterSize);
  }
}
