#ifndef ITEXTUREMANAGER_HPP
#define ITEXTUREMANAGER_HPP

#include "ITexture.hpp"
#include <memory>
#include <string>

namespace rtype::engine {
  class ITextureManager {
  public:
    virtual ~ITextureManager() = default;

    virtual std::shared_ptr<ITexture> loadTexture(const std::string& filepath) = 0;
    virtual void clear() = 0;
  };
}

#endif // !ITEXTUREMANAGER_HPP
