#ifndef SFMLTEXTURE_HPP
#define SFMLTEXTURE_HPP

#include "ITexture.hpp"
#include <SFML/Graphics/Texture.hpp>

namespace rtype::engine {
  class SFMLTexture : public ITexture {
  public:
    SFMLTexture() = default;
    ~SFMLTexture() override = default;

    bool loadFromFile(const std::string& filepath) override;
    Vector2u getSize() const override;

    const sf::Texture& getNativeTexture() const { return _texture; }

  private:
    sf::Texture _texture;
  };
}

#endif // !SFMLTEXTURE_HPP
