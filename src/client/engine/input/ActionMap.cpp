#include "engine/input/ActionMap.hpp"
#include "engine/config/Config.hpp"
#include "exception.hpp"
#include <algorithm>

namespace rtype::engine {
  namespace {
    const std::vector<KeyCode> NO_BINDINGS{};
  }

  void ActionMap::bind(const std::string& action, KeyCode key) {
    auto& keys = _bindings[action];
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
  }

  void ActionMap::clear(const std::string& action) {
    _bindings.erase(action);
  }

  void ActionMap::loadFromConfig(const Config& config, const std::string& section) {
    for (const auto& action : config.getKeys(section)) {
      clear(action);
      _bindings[action]; 
      for (const auto& keyName : config.getList(section, action)) {
        auto key = keyCodeFromString(keyName);
        if (!key)
          throw RtypeError(config.getOrigin() + ": [" + section + "] " + action + ": unknown key '" + keyName + "'");
        bind(action, *key);
      }
    }
  }

  bool ActionMap::has(const std::string& action) const {
    return _bindings.find(action) != _bindings.end();
  }

  bool ActionMap::isActive(const std::string& action) const {
    const auto& keys = getBindings(action);
    return std::any_of(keys.begin(), keys.end(), [this](KeyCode key) { return _input.isKeyPressed(key); });
  }

  bool ActionMap::isJustPressed(const std::string& action) const {
    const auto& keys = getBindings(action);
    return std::any_of(keys.begin(), keys.end(), [this](KeyCode key) { return _input.isKeyJustPressed(key); });
  }

  bool ActionMap::isJustReleased(const std::string& action) const {
    const auto& keys = getBindings(action);
    return std::any_of(keys.begin(), keys.end(), [this](KeyCode key) { return _input.isKeyJustReleased(key); });
  }

  const std::vector<KeyCode>& ActionMap::getBindings(const std::string& action) const {
    auto it = _bindings.find(action);
    return it != _bindings.end() ? it->second : NO_BINDINGS;
  }
}
