#ifndef RTYPE_ENGINE_ACTIONMAP_HPP
#define RTYPE_ENGINE_ACTIONMAP_HPP

#include "engine/input/IInputManager.hpp"
#include "engine/input/KeyCodes.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine {
  class Config;

  class ActionMap {
  public:
    explicit ActionMap(const IInputManager& input) : _input(input) {}

    void bind(const std::string& action, KeyCode key);
    void clear(const std::string& action);


    void loadFromConfig(const Config& config, const std::string& section);

    bool has(const std::string& action) const;
    bool isActive(const std::string& action) const;
    bool isJustPressed(const std::string& action) const;
    bool isJustReleased(const std::string& action) const;

    const std::vector<KeyCode>& getBindings(const std::string& action) const;

  private:
    const IInputManager& _input;
    std::unordered_map<std::string, std::vector<KeyCode>> _bindings;
  };
}

#endif // !RTYPE_ENGINE_ACTIONMAP_HPP
