#ifndef RTYPE_BACKEND_SFMLFONTMANAGER_HPP
#define RTYPE_BACKEND_SFMLFONTMANAGER_HPP

#include "engine/gfx/IFontManager.hpp"
#include <SFML/Graphics/Font.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::backend {
  class SFMLFontManager : public engine::IFontManager {
  public:
    SFMLFontManager() = default;
    ~SFMLFontManager() override = default;

    void loadFont(const std::string& filepath) override;
    std::shared_ptr<engine::IText> createText(const std::string& filepath) override;
    void clear() override;

  private:
    std::shared_ptr<const sf::Font> getFont(const std::string& filepath);

    std::unordered_map<std::string, std::shared_ptr<const sf::Font>> _cache;
  };
}

#endif // !RTYPE_BACKEND_SFMLFONTMANAGER_HPP
