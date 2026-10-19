#ifndef RTYPE_ENGINE_SESSION_HPP
#define RTYPE_ENGINE_SESSION_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rtype::engine {
  class Session {
  public:
    void set(const std::string& key, const std::string& value) { _values[key] = value; }
    void setInt(const std::string& key, int value) { _values[key] = std::to_string(value); }

    bool has(const std::string& key) const { return _values.find(key) != _values.end(); }

    std::optional<std::string> get(const std::string& key) const {
      auto it = _values.find(key);
      if (it == _values.end()) return std::nullopt;
      return it->second;
    }

    int getInt(const std::string& key, int fallback) const {
      auto it = _values.find(key);
      if (it == _values.end()) return fallback;
      try {
        return std::stoi(it->second);
      } catch (const std::logic_error&) {
        return fallback;
      }
    }

  private:
    std::unordered_map<std::string, std::string> _values;
  };
}

#endif // !RTYPE_ENGINE_SESSION_HPP
