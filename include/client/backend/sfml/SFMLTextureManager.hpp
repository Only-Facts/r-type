#ifndef RTYPE_BACKEND_SFMLTEXTUREMANAGER_HPP
#define RTYPE_BACKEND_SFMLTEXTUREMANAGER_HPP

#include "engine/gfx/ITextureManager.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::backend {
  class SFMLTextureManager : public engine::ITextureManager {
  public:
    SFMLTextureManager() = default;
    ~SFMLTextureManager() override = default;

    std::shared_ptr<engine::ITexture> loadTexture(const std::string& filepath) override;
    std::shared_ptr<engine::ISprite> createSprite(const std::string& filepath) override;
    void clear() override;

  private:
    std::unordered_map<std::string, std::shared_ptr<engine::ITexture>> _cache;
  };
}

#endif // !RTYPE_BACKEND_SFMLTEXTUREMANAGER_HPP
