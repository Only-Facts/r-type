#ifndef RTYPE_GAME_ENTITYCATALOG_HPP
#define RTYPE_GAME_ENTITYCATALOG_HPP

#include "engine/config/Config.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace rtype::game {
  struct EntityProfile {
    std::string name;
    std::string asset;
    int zIndex{0};
    float scale{1.0f};
    float interpolationSpeed{15.0f};
    std::string spawnSound;
    std::string destroySound;
    std::string hitSound;
  };

  class EntityCatalog {
  public:
    void load(const engine::Config& config, const std::string& section);
    const EntityProfile* find(std::uint8_t type) const;
    std::size_t size() const { return _profiles.size(); }

  private:
    std::unordered_map<std::uint8_t, EntityProfile> _profiles;
  };
}

#endif // !RTYPE_GAME_ENTITYCATALOG_HPP
