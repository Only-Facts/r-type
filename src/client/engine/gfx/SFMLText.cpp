#include "gfx/SFMLText.hpp"
#include <utility>

namespace rtype::engine {
  SFMLText::SFMLText(std::shared_ptr<const sf::Font> font, unsigned int characterSize)
    : _fontOwner(std::move(font)), _text(*_fontOwner, "", characterSize) {}

  void SFMLText::setString(const std::string& text) {
    _text.setString(text);
  }

  void SFMLText::setPosition(const Vector2f& position) {
    _text.setPosition({position.x, position.y});
  }

  void SFMLText::setColor(const Color& color) {
    _text.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
  }

  Vector2f SFMLText::getSize() const {
    const sf::FloatRect bounds = _text.getLocalBounds();
    return Vector2f{bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
  }
}
