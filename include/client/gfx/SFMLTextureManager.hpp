#ifndef SFMLTEXTUREMANAGER_HPP
#define SFMLTEXTUREMANAGER_HPP

#include "ITextureManager.hpp"
#include <unordered_map>

namespace rtype::engine {
  class SFMLTextureManager : public ITextureManager {
  public:
    SFMLTextureManager() = default;
    ~SFMLTextureManager() override = default;

    std::shared_ptr<ITexture> loadTexture(const std::string& filepath) override;
    void clear() override;

  private:
    std::unordered_map<std::string, std::shared_ptr<ITexture>> _cache;
  };
}

#endif // !SFMLTEXTUREMANAGER_HPP
