#ifndef RTYPE_GAME_AUGMENTCATALOG_HPP
#define RTYPE_GAME_AUGMENTCATALOG_HPP

#include "engine/config/Config.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace rtype::game {
  struct AugmentProfile {
    std::string name;
    std::string label;
    engine::Color color{255, 255, 255, 255};
    std::string gainSound;
    std::string endSound;
  };

  class AugmentCatalog {
  public:
    void load(const engine::Config& config, const std::string& section);
    const AugmentProfile* find(std::uint8_t id) const;
    std::size_t size() const { return _profiles.size(); }

    template<typename Func>
    void forEach(Func&& func) const {
      for (const auto& [id, profile] : _profiles) func(id, profile);
    }

  private:
    std::unordered_map<std::uint8_t, AugmentProfile> _profiles;
  };
}

#endif // !RTYPE_GAME_AUGMENTCATALOG_HPP
