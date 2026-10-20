#include "game/SkinCatalog.hpp"
#include "exception.hpp"
#include <algorithm>
#include <utility>

namespace rtype::game {
  void SkinCatalog::load(const engine::Config& config, const std::string& section) {
    _skins.clear();
    for (const auto& id : config.getList(section, "list")) {
      const std::string skinSection = "skin:" + id;
      if (!config.hasSection(skinSection))
        throw RtypeError("[" + section + "] list: missing [" + skinSection + "] section");
      Skin skin;
      skin.id = id;
      skin.label = config.getString(skinSection, "label", id);
      skin.asset = config.getString(skinSection, "asset");
      skin.color = config.getColor(skinSection, "color", skin.color);
      if (skin.asset.empty()) throw RtypeError("[" + skinSection + "] has no 'asset'");
      _skins.push_back(std::move(skin));
    }
    if (_skins.size() > 255) throw RtypeError("[" + section + "] at most 255 skins");
    if (_skins.empty()) return;

    const std::string wanted = config.getString(section, "default", _skins.front().id);
    auto it = std::find_if(_skins.begin(), _skins.end(), [&wanted](const Skin& skin) { return skin.id == wanted; });
    if (it == _skins.end()) throw RtypeError("[" + section + "] default '" + wanted + "' is not in 'list'");
    _defaultIndex = static_cast<std::size_t>(it - _skins.begin());
  }

  const Skin& SkinCatalog::at(std::size_t index) const {
    if (index >= _skins.size()) throw RtypeError("Unknown skin index " + std::to_string(index));
    return _skins[index];
  }

  std::optional<std::size_t> SkinCatalog::find(std::size_t wireValue) const {
    if (wireValue >= _skins.size()) return std::nullopt;
    return wireValue;
  }

  std::size_t SkinCatalog::selected(const engine::Session& session) const {
    const int index = session.getInt(SESSION_KEY, static_cast<int>(_defaultIndex));
    if (index < 0 || static_cast<std::size_t>(index) >= _skins.size()) return _defaultIndex;
    return static_cast<std::size_t>(index);
  }

  void SkinCatalog::select(engine::Session& session, std::size_t index) const {
    if (_skins.empty()) return;
    session.setInt(SESSION_KEY, static_cast<int>(std::min(index, _skins.size() - 1)));
  }
}
