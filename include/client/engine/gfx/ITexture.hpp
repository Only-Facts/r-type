#ifndef RTYPE_ENGINE_ITEXTURE_HPP
#define RTYPE_ENGINE_ITEXTURE_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <string>

namespace rtype::engine {
  /// Backend-agnostic texture. Only the backend knows the concrete type.
  class ITexture {
  public:
    virtual ~ITexture() = default;

    virtual bool loadFromFile(const std::string& filepath) = 0;
    virtual Vector2u getSize() const = 0;
  };
}

#endif // !RTYPE_ENGINE_ITEXTURE_HPP
