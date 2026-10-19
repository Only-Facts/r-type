#include "game/Difficulty.hpp"
#include "exception.hpp"
#include <algorithm>
#include <utility>

namespace rtype::game {
  void Difficulty::load(const engine::Config& config, const std::string& section) {
    _levels.clear();
    for (const auto& id : config.getList(section, "levels")) {
      DifficultyLevel level;
      level.id = id;
      level.label = config.getString(section, "label_" + id, id);
      level.color = config.getColor(section, "color_" + id, level.color);
      _levels.push_back(std::move(level));
    }
    if (_levels.empty()) {
      _levels.push_back(DifficultyLevel{"normal", "Normal", engine::Color{255, 255, 255, 255}});
    }
    if (_levels.size() > 255) throw RtypeError("[" + section + "] at most 255 levels");

    const std::string wanted = config.getString(section, "default", _levels.front().id);
    auto it = std::find_if(_levels.begin(), _levels.end(), [&wanted](const DifficultyLevel& level) {
      return level.id == wanted;
    });
    if (it == _levels.end()) throw RtypeError("[" + section + "] default '" + wanted + "' is not in 'levels'");
    _defaultIndex = static_cast<std::size_t>(it - _levels.begin());
  }

  const DifficultyLevel& Difficulty::at(std::size_t index) const {
    if (index >= _levels.size()) throw RtypeError("Unknown difficulty index " + std::to_string(index));
    return _levels[index];
  }

  std::size_t Difficulty::selected(const engine::Session& session) const {
    const int index = session.getInt(SESSION_KEY, static_cast<int>(_defaultIndex));
    if (index < 0 || static_cast<std::size_t>(index) >= _levels.size()) return _defaultIndex;
    return static_cast<std::size_t>(index);
  }

  void Difficulty::select(engine::Session& session, std::size_t index) const {
    session.setInt(SESSION_KEY, static_cast<int>(std::min(index, _levels.size() - 1)));
  }
}
