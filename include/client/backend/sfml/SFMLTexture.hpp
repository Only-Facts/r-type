#ifndef RTYPE_BACKEND_SFMLTEXTURE_HPP
#define RTYPE_BACKEND_SFMLTEXTURE_HPP

#include "engine/gfx/ITexture.hpp"
#include <SFML/Graphics/Texture.hpp>
#include <string>

namespace rtype::backend {
  class SFMLTexture : public engine::ITexture {
  public:
    SFMLTexture() = default;
    ~SFMLTexture() override = default;

    bool loadFromFile(const std::string& filepath) override;
    engine::Vector2u getSize() const override;

    const sf::Texture& getNativeTexture() const { return _texture; }

  private:
    sf::Texture _texture;
  };
}

#endif // !RTYPE_BACKEND_SFMLTEXTURE_HPP
