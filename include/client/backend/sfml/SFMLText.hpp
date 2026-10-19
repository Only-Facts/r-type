#ifndef RTYPE_BACKEND_SFMLTEXT_HPP
#define RTYPE_BACKEND_SFMLTEXT_HPP

#include "engine/gfx/IText.hpp"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <memory>

namespace rtype::backend {
  class SFMLText : public engine::IText {
  public:
    explicit SFMLText(std::shared_ptr<const sf::Font> font);
    ~SFMLText() override = default;

    void setString(const std::string& text) override;
    void setCharacterSize(unsigned int size) override;
    void setPosition(const engine::Vector2f& position) override;
    void setColor(const engine::Color& color) override;
    void setOutline(const engine::Color& color, float thickness) override;
    engine::Vector2f getSize() const override;

    const sf::Text& getNative() const { return _text; }

  private:
    std::shared_ptr<const sf::Font> _font;
    sf::Text _text;
  };
}

#endif // !RTYPE_BACKEND_SFMLTEXT_HPP
