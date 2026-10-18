#ifndef RTYPE_ENGINE_CONFIG_HPP
#define RTYPE_ENGINE_CONFIG_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine {
  /// Minimal INI-style configuration, with no third-party dependency.
  ///
  ///   # comment
  ///   [section]
  ///   key = value
  ///
  /// Getters with a fallback return it when the key is missing, and throw
  /// RtypeError when the value exists but is malformed (fail fast, with file:line).
  class Config {
  public:
    Config() = default;

    static Config fromFile(const std::string& path);
    static Config fromString(const std::string& content, const std::string& origin = "<memory>");

    bool hasSection(const std::string& section) const;
    bool has(const std::string& section, const std::string& key) const;
    void set(const std::string& section, const std::string& key, const std::string& value);

    std::string getString(const std::string& section, const std::string& key, const std::string& fallback = "") const;
    int getInt(const std::string& section, const std::string& key, int fallback) const;
    float getFloat(const std::string& section, const std::string& key, float fallback) const;
    bool getBool(const std::string& section, const std::string& key, bool fallback) const;
    Color getColor(const std::string& section, const std::string& key, const Color& fallback) const;

    /// Whitespace-separated values: `speeds = 40 100 220`.
    std::vector<std::string> getList(const std::string& section, const std::string& key) const;
    std::vector<float> getFloatList(const std::string& section, const std::string& key) const;
    std::vector<int> getIntList(const std::string& section, const std::string& key) const;

    /// Sections in file order (used by the asset manifest).
    const std::vector<std::string>& getSections() const { return _sectionOrder; }
    std::vector<std::string> getKeys(const std::string& section) const;
    const std::string& getOrigin() const { return _origin; }

  private:
    struct Entry {
      std::string value;
      std::size_t line{0};
    };

    const Entry* find(const std::string& section, const std::string& key) const;
    [[noreturn]] void fail(const std::string& section, const std::string& key, const std::string& reason) const;

    std::string _origin{"<memory>"};
    std::vector<std::string> _sectionOrder;
    std::unordered_map<std::string, std::vector<std::string>> _keyOrder;
    std::unordered_map<std::string, std::unordered_map<std::string, Entry>> _data;
  };
}

#endif // !RTYPE_ENGINE_CONFIG_HPP
