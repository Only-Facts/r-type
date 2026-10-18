#ifndef RTYPE_ENGINE_ITEXTUREMANAGER_HPP
#define RTYPE_ENGINE_ITEXTUREMANAGER_HPP

#include "engine/gfx/ISprite.hpp"
#include "engine/gfx/ITexture.hpp"
#include <memory>
#include <string>

namespace rtype::engine {

  class ITextureManager {
  public:
    virtual ~ITextureManager() = default;

    virtual std::shared_ptr<ITexture> loadTexture(const std::string& filepath) = 0;
    virtual std::shared_ptr<ISprite> createSprite(const std::string& filepath) = 0;
    virtual void clear() = 0;
  };
}

#endif // !RTYPE_ENGINE_ITEXTUREMANAGER_HPP
