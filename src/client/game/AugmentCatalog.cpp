#include "game/AugmentCatalog.hpp"
#include "exception.hpp"
#include <stdexcept>
#include <utility>

namespace rtype::game {
  void AugmentCatalog::load(const engine::Config& config, const std::string& section) {
    _profiles.clear();
    for (const auto& key : config.getKeys(section)) {
      int id = -1;
      try {
        std::size_t consumed = 0;
        id = std::stoi(key, &consumed);
        if (consumed != key.size()) id = -1;
      } catch (const std::logic_error&) {
        id = -1;
      }
      if (id < 1 || id > 255)
        throw RtypeError("[" + section + "] '" + key + "' must be an augment id in [1, 255]");

      AugmentProfile profile;
      profile.name = config.getString(section, key);
      const std::string profileSection = "augment:" + profile.name;
      if (!config.hasSection(profileSection))
        throw RtypeError("[" + section + "] " + key + " = " + profile.name + ": missing [" + profileSection + "] section");

      profile.label = config.getString(profileSection, "label", profile.name);
      profile.color = config.getColor(profileSection, "color", profile.color);
      profile.gainSound = config.getString(profileSection, "gain_sound");
      profile.endSound = config.getString(profileSection, "end_sound");
      _profiles[static_cast<std::uint8_t>(id)] = std::move(profile);
    }
  }

  const AugmentProfile* AugmentCatalog::find(std::uint8_t id) const {
    auto it = _profiles.find(id);
    return it != _profiles.end() ? &it->second : nullptr;
  }
}
