#ifndef ITEXTURE_HPP
#define ITEXTURE_HPP

#include "GfxTypes.hpp"
#include <string>

namespace rtype::engine {
  class ITexture {
  public:
    virtual ~ITexture() = default;

    virtual bool loadFromFile(const std::string& filepath) = 0;
    virtual Vector2u getSize() const = 0;
  };
}

#endif // !ITEXTURE_HPP
