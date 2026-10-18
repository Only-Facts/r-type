#ifndef SFMLTEXTFACTORY_HPP
#define SFMLTEXTFACTORY_HPP

#include "ITextFactory.hpp"
#include <SFML/Graphics/Font.hpp>
#include <memory>
#include <string>

namespace rtype::engine {
  class SFMLTextFactory : public ITextFactory {
  public:
    explicit SFMLTextFactory(const std::string& fontPath);
    ~SFMLTextFactory() override = default;

    std::shared_ptr<IText> createText(unsigned int characterSize) override;

  private:
    std::shared_ptr<sf::Font> _font;
  };
}

#endif // !SFMLTEXTFACTORY_HPP
