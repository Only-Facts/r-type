#include "game/EntityCatalog.hpp"
#include "exception.hpp"
#include <stdexcept>
#include <utility>
namespace rtype::game {
  void EntityCatalog::load(const engine::Config& config, const std::string& section) {
    for (const auto& key : config.getKeys(section)) {
      int id = -1;
      try {
        std::size_t consumed = 0;
        id = std::stoi(key, &consumed);
        if (consumed != key.size()) id = -1;
      } catch (const std::logic_error&) {
        id = -1;
      }
      if (id < 0 || id > 255)
        throw RtypeError("[" + section + "] '" + key + "' must be an entity type id in [0, 255]");

      EntityProfile profile;
      profile.name = config.getString(section, key);
      const std::string profileSection = "entity:" + profile.name;
      if (!config.hasSection(profileSection))
        throw RtypeError("[" + section + "] " + key + " = " + profile.name + ": missing [" + profileSection + "] section");

      profile.asset = config.getString(profileSection, "asset");
      if (profile.asset.empty()) throw RtypeError("[" + profileSection + "] has no 'asset'");
      profile.zIndex = config.getInt(profileSection, "z", profile.zIndex);
      profile.scale = config.getFloat(profileSection, "scale", profile.scale);
      profile.interpolationSpeed = config.getFloat(profileSection, "interpolation_speed", profile.interpolationSpeed);
      profile.spawnSound = config.getString(profileSection, "spawn_sound");
      profile.destroySound = config.getString(profileSection, "destroy_sound");
      profile.hitSound = config.getString(profileSection, "hit_sound");

      _profiles[static_cast<std::uint8_t>(id)] = std::move(profile);
    }
  }

  const EntityProfile* EntityCatalog::find(std::uint8_t type) const {
    auto it = _profiles.find(type);
    return it != _profiles.end() ? &it->second : nullptr;
  }
}
