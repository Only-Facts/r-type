#include "engine/config/Config.hpp"
#include "exception.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace rtype::engine {
  namespace {
    std::string trim(const std::string& str) {
      const auto begin = str.find_first_not_of(" \t\r\n");
      if (begin == std::string::npos) return "";
      const auto end = str.find_last_not_of(" \t\r\n");
      return str.substr(begin, end - begin + 1);
    }

    std::string toLower(std::string str) {
      std::transform(str.begin(), str.end(), str.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return str;
    }
  }

  Config Config::fromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw RtypeError("Cannot open config file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromString(buffer.str(), path);
  }

  Config Config::fromString(const std::string& content, const std::string& origin) {
    Config config;
    config._origin = origin;

    std::istringstream stream(content);
    std::string rawLine;
    std::string section;
    std::size_t lineNumber = 0;

    while (std::getline(stream, rawLine)) {
      ++lineNumber;
      const std::string line = trim(rawLine);
      if (line.empty() || line[0] == '#' || line[0] == ';') continue;

      if (line.front() == '[') {
        if (line.back() != ']' || line.size() < 3)
          throw RtypeError(origin + ":" + std::to_string(lineNumber) + ": malformed section header");
        section = trim(line.substr(1, line.size() - 2));
        if (!config.hasSection(section)) {
          config._sectionOrder.push_back(section);
          config._data[section];
        }
        continue;
      }

      const auto equal = line.find('=');
      if (equal == std::string::npos)
        throw RtypeError(origin + ":" + std::to_string(lineNumber) + ": expected 'key = value'");
      if (section.empty())
        throw RtypeError(origin + ":" + std::to_string(lineNumber) + ": key outside of any [section]");

      const std::string key = trim(line.substr(0, equal));
      if (key.empty())
        throw RtypeError(origin + ":" + std::to_string(lineNumber) + ": empty key");

      auto& entries = config._data[section];
      if (entries.find(key) == entries.end()) config._keyOrder[section].push_back(key);
      entries[key] = Entry{trim(line.substr(equal + 1)), lineNumber};
    }
    return config;
  }

  bool Config::hasSection(const std::string& section) const {
    return _data.find(section) != _data.end();
  }

  bool Config::has(const std::string& section, const std::string& key) const {
    return find(section, key) != nullptr;
  }

  void Config::set(const std::string& section, const std::string& key, const std::string& value) {
    if (!hasSection(section)) _sectionOrder.push_back(section);
    auto& entries = _data[section];
    if (entries.find(key) == entries.end()) _keyOrder[section].push_back(key);
    entries[key] = Entry{value, 0};
  }

  std::string Config::getString(const std::string& section, const std::string& key, const std::string& fallback) const {
    const Entry* entry = find(section, key);
    return entry ? entry->value : fallback;
  }

  int Config::getInt(const std::string& section, const std::string& key, int fallback) const {
    const Entry* entry = find(section, key);
    if (!entry) return fallback;
    try {
      std::size_t consumed = 0;
      const int value = std::stoi(entry->value, &consumed);
      if (consumed != entry->value.size()) fail(section, key, "expected an integer, got '" + entry->value + "'");
      return value;
    } catch (const std::logic_error&) {
      fail(section, key, "expected an integer, got '" + entry->value + "'");
    }
  }

  float Config::getFloat(const std::string& section, const std::string& key, float fallback) const {
    const Entry* entry = find(section, key);
    if (!entry) return fallback;
    try {
      std::size_t consumed = 0;
      const float value = std::stof(entry->value, &consumed);
      if (consumed != entry->value.size()) fail(section, key, "expected a number, got '" + entry->value + "'");
      return value;
    } catch (const std::logic_error&) {
      fail(section, key, "expected a number, got '" + entry->value + "'");
    }
  }

  bool Config::getBool(const std::string& section, const std::string& key, bool fallback) const {
    const Entry* entry = find(section, key);
    if (!entry) return fallback;
    const std::string value = toLower(entry->value);
    if (value == "true" || value == "yes" || value == "on" || value == "1") return true;
    if (value == "false" || value == "no" || value == "off" || value == "0") return false;
    fail(section, key, "expected a boolean, got '" + entry->value + "'");
  }

  Color Config::getColor(const std::string& section, const std::string& key, const Color& fallback) const {
    if (!has(section, key)) return fallback;
    const auto values = getIntList(section, key);
    if (values.size() != 3 && values.size() != 4)
      fail(section, key, "expected 'r g b' or 'r g b a'");
    for (int v : values)
      if (v < 0 || v > 255) fail(section, key, "color components must be in [0, 255]");

    return Color{
      static_cast<std::uint8_t>(values[0]),
      static_cast<std::uint8_t>(values[1]),
      static_cast<std::uint8_t>(values[2]),
      static_cast<std::uint8_t>(values.size() == 4 ? values[3] : 255)
    };
  }

  std::vector<std::string> Config::getList(const std::string& section, const std::string& key) const {
    std::vector<std::string> result;
    const Entry* entry = find(section, key);
    if (!entry) return result;

    std::istringstream stream(entry->value);
    std::string token;
    while (stream >> token) result.push_back(token);
    return result;
  }

  std::vector<float> Config::getFloatList(const std::string& section, const std::string& key) const {
    std::vector<float> result;
    for (const auto& token : getList(section, key)) {
      try {
        std::size_t consumed = 0;
        result.push_back(std::stof(token, &consumed));
        if (consumed != token.size()) fail(section, key, "invalid number '" + token + "'");
      } catch (const std::logic_error&) {
        fail(section, key, "invalid number '" + token + "'");
      }
    }
    return result;
  }

  std::vector<int> Config::getIntList(const std::string& section, const std::string& key) const {
    std::vector<int> result;
    for (const auto& token : getList(section, key)) {
      try {
        std::size_t consumed = 0;
        result.push_back(std::stoi(token, &consumed));
        if (consumed != token.size()) fail(section, key, "invalid integer '" + token + "'");
      } catch (const std::logic_error&) {
        fail(section, key, "invalid integer '" + token + "'");
      }
    }
    return result;
  }

  std::vector<std::string> Config::getKeys(const std::string& section) const {
    auto it = _keyOrder.find(section);
    return it != _keyOrder.end() ? it->second : std::vector<std::string>{};
  }

  const Config::Entry* Config::find(const std::string& section, const std::string& key) const {
    auto sectionIt = _data.find(section);
    if (sectionIt == _data.end()) return nullptr;
    auto keyIt = sectionIt->second.find(key);
    return keyIt != sectionIt->second.end() ? &keyIt->second : nullptr;
  }

  void Config::fail(const std::string& section, const std::string& key, const std::string& reason) const {
    const Entry* entry = find(section, key);
    std::string where = _origin;
    if (entry && entry->line > 0) where += ":" + std::to_string(entry->line);
    throw RtypeError(where + ": [" + section + "] " + key + ": " + reason);
  }
}
