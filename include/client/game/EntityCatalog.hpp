#ifndef RTYPE_GAME_ENTITYCATALOG_HPP
#define RTYPE_GAME_ENTITYCATALOG_HPP

#include "engine/config/Config.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace rtype::game {
  /// How the client shows one networked entity type (Bydos, missiles...).
  struct EntityProfile {
    std::string name;
    std::string asset;
    int zIndex{0};
    float scale{1.0f};
    float interpolationSpeed{15.0f};  ///< 0 = snap to the server position.
    std::string spawnSound;
    std::string destroySound;
    std::string hitSound;
  };

  /// Network entity type id -> profile, read from the config:
  ///
  ///   [entity_types]
  ///   1 = bydos_slave
  ///
  ///   [entity:bydos_slave]
  ///   asset = enemy
  ///   destroy_sound = sfx_explosion
  ///
  /// Adding an enemy or a weapon on the client = a few config lines, no code.
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
