#ifndef RTYPE_GAME_SKINCATALOG_HPP
#define RTYPE_GAME_SKINCATALOG_HPP

#include "engine/config/Config.hpp"
#include "engine/core/Session.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rtype::game {
  struct Skin {
    std::string id;
    std::string label;
    std::string asset;
    engine::Color color{255, 255, 255, 255};
  };

  class SkinCatalog {
  public:
    static constexpr const char* SESSION_KEY = "skin";

    void load(const engine::Config& config, const std::string& section);

    std::size_t count() const { return _skins.size(); }
    bool empty() const { return _skins.empty(); }
    const Skin& at(std::size_t index) const;
    std::optional<std::size_t> find(std::size_t wireValue) const;

    std::size_t selected(const engine::Session& session) const;
    void select(engine::Session& session, std::size_t index) const;

  private:
    std::vector<Skin> _skins;
    std::size_t _defaultIndex{0};
  };
}

#endif // !RTYPE_GAME_SKINCATALOG_HPP
