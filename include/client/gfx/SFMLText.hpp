#ifndef SFMLTEXT_HPP
#define SFMLTEXT_HPP

#include "IText.hpp"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <memory>

namespace rtype::engine {
  class SFMLText : public IText {
  public:
    SFMLText(std::shared_ptr<const sf::Font> font, unsigned int characterSize);
    ~SFMLText() override = default;

    void setString(const std::string& text) override;
    void setPosition(const Vector2f& position) override;
    void setColor(const Color& color) override;

    Vector2f getSize() const override;

    const sf::Text& getNativeText() const { return _text; }

  private:
    std::shared_ptr<const sf::Font> _fontOwner;
    sf::Text _text;
  };
}

#endif // !SFMLTEXT_HPP
