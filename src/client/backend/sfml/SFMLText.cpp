#include "backend/sfml/SFMLText.hpp"

namespace rtype::backend {
  SFMLText::SFMLText(std::shared_ptr<const sf::Font> font)
    : _font(std::move(font)), _text(*_font) {}

  void SFMLText::setString(const std::string& text) {
    // Config strings are UTF-8 (accents in translated HUD texts must render).
    _text.setString(sf::String::fromUtf8(text.begin(), text.end()));
  }

  void SFMLText::setCharacterSize(unsigned int size) {
    _text.setCharacterSize(size);
  }

  void SFMLText::setPosition(const engine::Vector2f& position) {
    _text.setPosition({position.x, position.y});
  }

  void SFMLText::setColor(const engine::Color& color) {
    _text.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
  }

  void SFMLText::setOutline(const engine::Color& color, float thickness) {
    _text.setOutlineColor(sf::Color(color.r, color.g, color.b, color.a));
    _text.setOutlineThickness(thickness);
  }

  engine::Vector2f SFMLText::getSize() const {
    const sf::FloatRect bounds = _text.getLocalBounds();
    return engine::Vector2f{bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
  }
}
